// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/mbs.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "ofod/random/normal_generator.h"

namespace ofod::pricers {
namespace {

constexpr double kMonth = 1.0 / 12.0;

// Level monthly payment on `balance` at monthly rate `i` over `n` months.
double LevelPayment(double balance, double i, int n) {
  if (i == 0.0) return balance / n;
  return balance * i / -std::expm1(-n * std::log1p(i));
}

struct MonthFlow {
  double interest;
  double scheduled;
  double prepaid;
};

// One month of the pool with `remaining` months left and SMM `smm`.
MonthFlow Step(const MortgagePool& pool, double balance, int remaining,
               double smm) {
  const double i_mortgage = pool.mortgage_rate / 12.0;
  const double payment = LevelPayment(balance, i_mortgage, remaining);
  const double scheduled = std::min(payment - balance * i_mortgage, balance);
  const double prepaid = smm * (balance - scheduled);
  return {.interest = balance * pool.pass_through_rate / 12.0,
          .scheduled = scheduled,
          .prepaid = prepaid};
}

}  // namespace

double PsaCpr(int month, double speed) {
  return 0.06 * speed / 100.0 * std::min(month / 30.0, 1.0);
}

double MonthlyMortality(double cpr) {
  return 1.0 - std::pow(1.0 - cpr, kMonth);
}

std::vector<MbsCashFlow> PassThroughCashFlows(const MortgagePool& pool,
                                              double psa_speed) {
  std::vector<MbsCashFlow> flows;
  double balance = pool.balance;
  for (int m = 1; m <= pool.months && balance > 1e-12; ++m) {
    const double smm = MonthlyMortality(PsaCpr(pool.age + m, psa_speed));
    const MonthFlow f = Step(pool, balance, pool.months - m + 1, smm);
    balance -= f.scheduled + f.prepaid;
    flows.push_back({.month = m,
                     .interest = f.interest,
                     .scheduled = f.scheduled,
                     .prepaid = f.prepaid,
                     .balance = balance});
  }
  return flows;
}

MbsValue ValueCashFlows(const std::vector<MbsCashFlow>& flows,
                        const curves::ZeroCurve& curve) {
  MbsValue v{0.0, 0.0, 0.0};
  for (const MbsCashFlow& f : flows) {
    const double df = curve.DiscountFactor(f.month * kMonth);
    v.interest_only += df * f.interest;
    v.principal_only += df * (f.scheduled + f.prepaid);
  }
  v.pass_through = v.interest_only + v.principal_only;
  return v;
}

std::optional<MbsValue> MbsMonteCarlo(const MortgagePool& pool,
                                      const models::HullWhite& rates,
                                      const RefinancingModel& model,
                                      const MbsMcOptions& options) {
  if (options.paths <= 0 || pool.months <= 0) return std::nullopt;
  const double a = rates.a();
  const double sigma = rates.sigma();
  // r(t) = x(t) + phi(t) with x an OU process from 0 and phi(t) = F(0, t)
  // + sigma^2 / 2 B(0, t)^2 (Brigo-Mercurio); the monthly x transition is
  // exact.
  const double decay = a > 0.0 ? std::exp(-a * kMonth) : 1.0;
  const double x_sd =
      sigma *
      std::sqrt(a > 0.0 ? -std::expm1(-2.0 * a * kMonth) / (2.0 * a) : kMonth);
  std::vector<double> phi(static_cast<std::size_t>(pool.months) + 1);
  for (int m = 0; m <= pool.months; ++m) {
    const double t = m * kMonth;
    const double b = rates.B(0.0, t);
    phi[static_cast<std::size_t>(m)] =
        rates.InstantaneousForward(t) + 0.5 * sigma * sigma * b * b;
  }
  random::NormalGenerator rng(options.seed);
  MbsValue total{0.0, 0.0, 0.0};
  for (int p = 0; p < options.paths; ++p) {
    double x = 0.0;
    double r = phi[0];
    double log_df = 0.0;
    double balance = pool.balance;
    for (int m = 1; m <= pool.months && balance > 1e-12; ++m) {
      // Trapezoidal discounting over the month.
      const double r_start = r;
      x = x * decay + x_sd * rng.Next();
      r = x + phi[static_cast<std::size_t>(m)];
      log_df -= 0.5 * (r_start + r) * kMonth;
      const double incentive =
          std::max(pool.mortgage_rate - (r_start + model.spread), 0.0);
      const double seasoning = std::min((pool.age + m) / 30.0, 1.0);
      const double cpr =
          std::min(seasoning * (model.base_cpr + model.sensitivity * incentive),
                   model.max_cpr);
      const MonthFlow f =
          Step(pool, balance, pool.months - m + 1, MonthlyMortality(cpr));
      balance -= f.scheduled + f.prepaid;
      const double df = std::exp(log_df);
      total.interest_only += df * f.interest;
      total.principal_only += df * (f.scheduled + f.prepaid);
    }
  }
  const double n = options.paths;
  total.interest_only /= n;
  total.principal_only /= n;
  total.pass_through = total.interest_only + total.principal_only;
  return total;
}

}  // namespace ofod::pricers
