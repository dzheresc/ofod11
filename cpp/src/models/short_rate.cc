// Copyright 2026 Denis Zhereshchin

#include "ofod/models/short_rate.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "ofod/math/normal.h"
#include "ofod/math/optimize.h"
#include "ofod/math/roots.h"

namespace ofod::models {
namespace {

using math::NormCdf;

// Hull-White calibration keeps a and sigma positive through exp().
constexpr double kMinA = 1e-6;

}  // namespace

double BondPrice(const Vasicek& m, double r, double tau) {
  const double b = (1.0 - std::exp(-m.a * tau)) / m.a;
  const double log_a =
      (b - tau) * (m.a * m.a * m.b - 0.5 * m.sigma * m.sigma) / (m.a * m.a) -
      m.sigma * m.sigma * b * b / (4.0 * m.a);
  return std::exp(log_a - b * r);
}

double BondPrice(const Cir& m, double r, double tau) {
  const double gamma = std::sqrt(m.a * m.a + 2.0 * m.sigma * m.sigma);
  const double e = std::expm1(gamma * tau);
  const double denom = (gamma + m.a) * e + 2.0 * gamma;
  const double b = 2.0 * e / denom;
  const double log_a =
      2.0 * m.a * m.b / (m.sigma * m.sigma) *
      std::log(2.0 * gamma * std::exp(0.5 * (m.a + gamma) * tau) / denom);
  return std::exp(log_a - b * r);
}

double ZeroRate(const Vasicek& m, double r, double tau) {
  return -std::log(BondPrice(m, r, tau)) / tau;
}

double ZeroRate(const Cir& m, double r, double tau) {
  return -std::log(BondPrice(m, r, tau)) / tau;
}

double ZeroBondOptionFormula(OptionType type, double p_expiry,
                             double p_maturity, double strike, double sigma_p) {
  const double phi = type == OptionType::kCall ? 1.0 : -1.0;
  const double h =
      std::log(p_maturity / (p_expiry * strike)) / sigma_p + 0.5 * sigma_p;
  return phi * (p_maturity * NormCdf(phi * h) -
                strike * p_expiry * NormCdf(phi * (h - sigma_p)));
}

double ZeroBondOptionPrice(const Vasicek& m, OptionType type, double r,
                           double expiry, double maturity, double strike) {
  const double sigma_p =
      m.sigma / m.a * (1.0 - std::exp(-m.a * (maturity - expiry))) *
      std::sqrt(-std::expm1(-2.0 * m.a * expiry) / (2.0 * m.a));
  return ZeroBondOptionFormula(type, BondPrice(m, r, expiry),
                               BondPrice(m, r, maturity), strike, sigma_p);
}

std::optional<Vasicek> EstimateVasicek(std::span<const double> rates,
                                       double dt) {
  if (rates.size() < 3 || dt <= 0.0) return std::nullopt;
  const std::size_t n = rates.size() - 1;
  double sx = 0.0;
  double sy = 0.0;
  double sxx = 0.0;
  double sxy = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    sx += rates[i];
    sy += rates[i + 1];
    sxx += rates[i] * rates[i];
    sxy += rates[i] * rates[i + 1];
  }
  const auto count = static_cast<double>(n);
  const double phi = (count * sxy - sx * sy) / (count * sxx - sx * sx);
  if (!(phi > 0.0 && phi < 1.0)) return std::nullopt;
  const double c = (sy - phi * sx) / count;
  double sse = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double e = rates[i + 1] - c - phi * rates[i];
    sse += e * e;
  }
  const double a = -std::log(phi) / dt;
  const double residual_var = sse / count;
  return Vasicek{
      .a = a,
      .b = c / (1.0 - phi),
      .sigma = std::sqrt(residual_var * 2.0 * a / (1.0 - phi * phi))};
}

std::vector<double> SimulateCir(const Cir& m, double r0, double dt, int steps,
                                std::uint64_t seed) {
  std::mt19937_64 engine(seed);
  const double decay = std::exp(-m.a * dt);
  const double c = m.sigma * m.sigma * (1.0 - decay) / (4.0 * m.a);
  const double dof = 4.0 * m.a * m.b / (m.sigma * m.sigma);
  std::vector<double> path;
  path.reserve(static_cast<std::size_t>(steps) + 1);
  path.push_back(r0);
  double r = r0;
  for (int i = 0; i < steps; ++i) {
    const double lambda = r * decay / c;
    std::poisson_distribution<int> poisson(0.5 * lambda);
    const int k = lambda > 0.0 ? poisson(engine) : 0;
    std::gamma_distribution<double> gamma(0.5 * dof + k, 2.0);
    r = c * gamma(engine);
    path.push_back(r);
  }
  return path;
}

std::optional<HullWhite> HullWhite::Create(curves::ZeroCurve curve, double a,
                                           double sigma) {
  if (!(a >= 0.0) || !(sigma > 0.0)) return std::nullopt;
  return HullWhite(std::move(curve), a, sigma);
}

double HullWhite::B(double t, double maturity) const {
  const double tau = maturity - t;
  if (a_ < 1e-10) return tau;
  return -std::expm1(-a_ * tau) / a_;
}

double HullWhite::InstantaneousForward(double t) const {
  // ln P is piecewise smooth for the linearly interpolated zero curve; a
  // short central difference gives F(0, t) = -d ln P / dt.
  const double h = 1e-5;
  const double lo = std::max(t - h, 0.0);
  const double hi = t + h;
  return (std::log(curve_.DiscountFactor(lo)) -
          std::log(curve_.DiscountFactor(hi))) /
         (hi - lo);
}

double HullWhite::BondPrice(double t, double maturity, double r) const {
  const double b = B(t, maturity);
  const double variance_term =
      a_ < 1e-10
          ? sigma_ * sigma_ * t * b * b / 2.0
          : sigma_ * sigma_ / (4.0 * a_) * -std::expm1(-2.0 * a_ * t) * b * b;
  const double log_a =
      std::log(curve_.DiscountFactor(maturity) / curve_.DiscountFactor(t)) +
      b * InstantaneousForward(t) - variance_term;
  return std::exp(log_a - b * r);
}

double HullWhite::BondOptionVolatility(double expiry, double maturity) const {
  if (a_ < 1e-10) return sigma_ * (maturity - expiry) * std::sqrt(expiry);
  return sigma_ * B(expiry, maturity) *
         std::sqrt(-std::expm1(-2.0 * a_ * expiry) / (2.0 * a_));
}

double HullWhite::ZeroBondOption(OptionType type, double expiry,
                                 double maturity, double strike) const {
  return ZeroBondOptionFormula(type, curve_.DiscountFactor(expiry),
                               curve_.DiscountFactor(maturity), strike,
                               BondOptionVolatility(expiry, maturity));
}

std::optional<double> HullWhite::CouponBondOption(
    OptionType type, double expiry, std::span<const double> times,
    std::span<const double> amounts, double strike) const {
  if (times.size() != amounts.size() || times.empty()) return std::nullopt;
  auto bond = [&](double r) {
    double value = 0.0;
    for (std::size_t i = 0; i < times.size(); ++i) {
      value += amounts[i] * BondPrice(expiry, times[i], r);
    }
    return value - strike;
  };
  const auto critical = math::Brent(bond, -1.0, 2.0);
  if (!critical) return std::nullopt;
  double price = 0.0;
  for (std::size_t i = 0; i < times.size(); ++i) {
    const double k = BondPrice(expiry, times[i], *critical);
    price += amounts[i] * ZeroBondOption(type, expiry, times[i], k);
  }
  return price;
}

double HullWhite::Caplet(OptionType type, double reset, double accrual,
                         double strike, double notional) const {
  const double growth = 1.0 + strike * accrual;
  const OptionType bond_type =
      type == OptionType::kCall ? OptionType::kPut : OptionType::kCall;
  return notional * growth *
         ZeroBondOption(bond_type, reset, reset + accrual, 1.0 / growth);
}

std::optional<HullWhiteFit> CalibrateHullWhite(
    const curves::ZeroCurve& curve, std::span<const CapletQuote> quotes,
    double a0, double sigma0) {
  if (quotes.empty() || a0 <= 0.0 || sigma0 <= 0.0) return std::nullopt;
  auto error = [&](std::span<const double> x) {
    const auto model =
        HullWhite::Create(curve, std::exp(x[0]) + kMinA, std::exp(x[1]));
    if (!model) return 1e10;
    double sum = 0.0;
    for (const CapletQuote& q : quotes) {
      const double e =
          model->Caplet(OptionType::kCall, q.reset, q.accrual, q.strike, 1.0) /
              q.market_price -
          1.0;
      sum += e * e;
    }
    return sum;
  };
  const std::vector<double> x0 = {std::log(a0), std::log(sigma0)};
  const std::vector<double> step = {0.5, 0.5};
  const math::Minimum best = math::NelderMead(error, x0, step);
  if (!std::isfinite(best.value)) return std::nullopt;
  return HullWhiteFit{.a = std::exp(best.x[0]) + kMinA,
                      .sigma = std::exp(best.x[1]),
                      .rms_relative_error = std::sqrt(
                          best.value / static_cast<double>(quotes.size()))};
}

}  // namespace ofod::models
