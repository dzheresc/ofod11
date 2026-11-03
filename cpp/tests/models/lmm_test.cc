// Copyright 2026 Denis Zhereshchin
//
// HJM, the LIBOR market model and agency MBS (Hull, Ch 33).

#include "ofod/models/lmm.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/models/short_rate.h"
#include "ofod/pricers/mbs.h"
#include "ofod/pricers/rate_options.h"

namespace ofod::models {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using instruments::OptionType;

LmmSpec Spec(int factors) {
  // Ten annual forwards rising from 5% to 6.8%, with a humped Lambda.
  LmmSpec spec{.accrual = 1.0, .forwards = {}, .loadings = {}};
  for (int k = 0; k < 10; ++k) spec.forwards.push_back(0.05 + 0.002 * k);
  for (int i = 0; i < 9; ++i) {
    const double lambda = 0.15 + 0.06 * std::exp(-0.5 * i) - 0.03 * i / 9.0;
    if (factors == 1) {
      spec.loadings.push_back({lambda});
    } else {
      // Two factors with a correlation decaying in maturity.
      const double angle = 0.15 * i;
      spec.loadings.push_back(
          {lambda * std::cos(angle), lambda * std::sin(angle)});
    }
  }
  return spec;
}

TEST_CASE("LMM volatilities from caplet volatilities", "[ch33]") {
  // Hull's example: caplet vols 24%, 22%, 20% give 24%, 19.80%, 15.23%.
  const auto lambdas =
      LmmVolatilitiesFromCaplets(std::vector<double>{0.24, 0.22, 0.20});
  REQUIRE(lambdas.has_value());
  CHECK_THAT((*lambdas)[0], WithinAbs(0.24, 1e-12));
  CHECK_THAT((*lambdas)[1], WithinAbs(0.1980, 5e-5));
  CHECK_THAT((*lambdas)[2], WithinAbs(0.1523, 5e-5));
  CHECK_FALSE(LmmVolatilitiesFromCaplets(std::vector<double>{0.3, 0.1}));
}

TEST_CASE("LMM prices zero bonds and caplets consistently", "[ch33]") {
  for (int factors : {1, 2}) {
    const LmmSpec spec = Spec(factors);
    const std::vector<double> p = LmmDiscountFactors(spec);
    // A unit paid at t_n is worth P(0, t_n).
    const LmmPayoff bond = [](const LmmPath& path) {
      return 1.0 / path.Numeraire(path.size());
    };
    const auto mc = LmmMonteCarlo(
        spec, bond, {.paths = 20'000, .seed = 5, .predictor_corrector = true});
    REQUIRE(mc.has_value());
    CHECK_THAT(mc->price, WithinAbs(p.back(), 3.0 * mc->std_error + 2e-4));
    // A caplet on F_6 matches Black with the root-mean-square Lambda.
    const int k = 6;
    double variance = 0.0;
    for (int i = 0; i < k; ++i) {
      for (double x : spec.loadings[static_cast<std::size_t>(i)]) {
        variance += x * x;
      }
    }
    const double vol = std::sqrt(variance / k);
    const double black = pricers::BlackCapletPrice(
        OptionType::kCall, spec.forwards[k], 0.06, vol, k, 1.0,
        p[static_cast<std::size_t>(k + 1)], 1.0);
    const LmmPayoff caplet = [k](const LmmPath& path) {
      return std::max(path.Reset(k) - 0.06, 0.0) / path.Numeraire(k + 1);
    };
    const auto mc_caplet = LmmMonteCarlo(
        spec, caplet,
        {.paths = 40'000, .seed = 7, .predictor_corrector = true});
    CHECK_THAT(mc_caplet->price, WithinAbs(black, 4.0 * mc_caplet->std_error));
  }
  CHECK_FALSE(
      LmmMonteCarlo({.accrual = 1.0, .forwards = {0.05, 0.05}, .loadings = {}},
                    [](const LmmPath&) { return 0.0; }, {}));
}

TEST_CASE("Rebonato's swaption volatility matches LMM Monte Carlo", "[ch33]") {
  const LmmSpec spec = Spec(2);
  const int start = 3;
  const int end = 8;
  const auto vol = RebonatoSwaptionVolatility(spec, start, end);
  REQUIRE(vol.has_value());
  const std::vector<double> p = LmmDiscountFactors(spec);
  double annuity = 0.0;
  for (int k = start; k < end; ++k)
    annuity += p[static_cast<std::size_t>(k + 1)];
  const double s0 = (p[start] - p[end]) / annuity;
  const double black = pricers::BlackSwaptionPrice(OptionType::kCall, s0, s0,
                                                   *vol, start, annuity, 1.0);
  const LmmPayoff swaption = [&](const LmmPath& path) {
    // Value of the swap at t_start from the simulated forwards.
    double df = 1.0;
    double a = 0.0;
    for (int k = start; k < end; ++k) {
      df /= 1.0 + path.Forward(start, k);
      a += df;
    }
    const double swap = 1.0 - df - s0 * a;
    return std::max(swap, 0.0) / path.Numeraire(start);
  };
  const auto mc =
      LmmMonteCarlo(spec, swaption,
                    {.paths = 40'000, .seed = 9, .predictor_corrector = true});
  CHECK_THAT(mc->price, WithinRel(black, 0.02));
  CHECK_FALSE(RebonatoSwaptionVolatility(spec, 0, 4));
}

TEST_CASE("Discrete HJM is arbitrage-free and matches Hull-White", "[ch33]") {
  const curves::ZeroCurve curve =
      *curves::ZeroCurve::Create({1.0, 5.0}, {0.03, 0.05});
  const double a = 0.1;
  const double sigma = 0.01;
  HjmSpec spec{.dt = 0.05, .forwards = {}, .volatility = nullptr};
  const HullWhite hw = *HullWhite::Create(curve, a, sigma);
  for (int i = 0; i < 100; ++i) {
    // Grid forward f(0, T_i) consistent with the discrete discounting.
    const double t = i * spec.dt;
    spec.forwards.push_back(
        std::log(curve.DiscountFactor(t) / curve.DiscountFactor(t + spec.dt)) /
        spec.dt);
  }
  spec.volatility = [a, sigma](double t, double maturity) {
    return sigma * std::exp(-a * (maturity - t));
  };
  const auto bond =
      HjmZeroBondOption(spec, OptionType::kCall, 40, 100, 0.0, 10'000, 3);
  REQUIRE(bond.has_value());
  CHECK_THAT(bond->price,
             WithinAbs(curve.DiscountFactor(5.0), 3.0 * bond->std_error));
  const auto option =
      HjmZeroBondOption(spec, OptionType::kPut, 40, 100, 0.82, 10'000, 4);
  CHECK_THAT(option->price,
             WithinAbs(hw.ZeroBondOption(OptionType::kPut, 2.0, 5.0, 0.82),
                       4.0 * option->std_error + 1e-4));
  CHECK_FALSE(HjmZeroBondOption(spec, OptionType::kPut, 40, 101, 0.8, 10, 1));
}

TEST_CASE("Pass-through cash flows and PSA prepayments", "[ch33]") {
  CHECK_THAT(pricers::PsaCpr(15, 100.0), WithinAbs(0.03, 1e-15));
  CHECK_THAT(pricers::PsaCpr(45, 150.0), WithinAbs(0.09, 1e-15));
  CHECK_THAT(pricers::MonthlyMortality(0.06), WithinAbs(0.0051430, 1e-7));
  const pricers::MortgagePool pool{.balance = 100.0,
                                   .mortgage_rate = 0.06,
                                   .pass_through_rate = 0.055,
                                   .months = 360};
  // No prepayments: a level annuity; principal sums to the balance.
  const auto level = pricers::PassThroughCashFlows(pool, 0.0);
  REQUIRE(level.size() == 360);
  double principal = 0.0;
  for (const auto& f : level) principal += f.scheduled + f.prepaid;
  CHECK_THAT(principal, WithinAbs(100.0, 1e-9));
  CHECK_THAT(level.back().balance, WithinAbs(0.0, 1e-9));
  // Faster prepayment shortens the life: IO loses, PO gains at 5%.
  const curves::ZeroCurve flat = curves::ZeroCurve::Flat(0.05);
  const auto slow =
      pricers::ValueCashFlows(pricers::PassThroughCashFlows(pool, 100.0), flat);
  const auto fast =
      pricers::ValueCashFlows(pricers::PassThroughCashFlows(pool, 300.0), flat);
  CHECK_THAT(slow.interest_only + slow.principal_only,
             WithinAbs(slow.pass_through, 1e-12));
  CHECK(fast.interest_only < slow.interest_only);
  CHECK(fast.principal_only > slow.principal_only);
  // With no rate sensitivity the Monte Carlo reproduces 100% PSA.
  const HullWhite hw = *HullWhite::Create(flat, 0.1, 0.01);
  const auto mc = pricers::MbsMonteCarlo(
      pool, hw, {.base_cpr = 0.06, .sensitivity = 0.0, .max_cpr = 1.0},
      {.paths = 4000, .seed = 2});
  REQUIRE(mc.has_value());
  CHECK_THAT(mc->pass_through, WithinRel(slow.pass_through, 2e-3));
  // Refinancing makes the MBS negatively convex: it gains less from a
  // fall in rates than it loses from a rise.
  auto value = [&](double shift) {
    const HullWhite shifted =
        *HullWhite::Create(curves::ZeroCurve::Flat(0.05 + shift), 0.1, 0.01);
    return pricers::MbsMonteCarlo(pool, shifted, {}, {.paths = 2000, .seed = 3})
        ->pass_through;
  };
  const double up = value(0.01);
  const double base = value(0.0);
  const double down = value(-0.01);
  CHECK(up + down - 2.0 * base < 0.0);
}

}  // namespace
}  // namespace ofod::models
