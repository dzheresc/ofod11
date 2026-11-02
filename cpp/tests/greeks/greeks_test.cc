// Copyright 2026 Denis Zhereshchin
//
// Greek letters, neutral hedges, scenario grids and the delta-hedging
// experiment (Hull, Ch 19).

#include "ofod/greeks/greeks.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "ofod/pricers/black_scholes.h"

namespace ofod::greeks {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using instruments::OptionType;

const OptionInputs kInputs{.spot = 42.0,
                           .rate = 0.08,
                           .yield = 0.03,
                           .volatility = 0.25,
                           .maturity = 0.75};

Pricer BsmPricer(OptionType type, double strike) {
  return [type, strike](const OptionInputs& x) {
    return pricers::BsmPrice(type, x.spot, strike, x.rate, x.yield,
                             x.volatility, x.maturity);
  };
}

TEST_CASE("Bumped Greeks agree with the closed forms", "[greeks][ch19]") {
  for (OptionType type : {OptionType::kCall, OptionType::kPut}) {
    const Greeks exact = BsmGreeks(type, 40.0, kInputs);
    const Greeks bumped = BumpGreeksErased(BsmPricer(type, 40.0), kInputs);
    CHECK_THAT(bumped.delta, WithinAbs(exact.delta, 1e-5));
    CHECK_THAT(bumped.gamma, WithinAbs(exact.gamma, 1e-5));
    CHECK_THAT(bumped.vega, WithinAbs(exact.vega, 1e-6));
    CHECK_THAT(bumped.theta, WithinAbs(exact.theta, 1e-4));
    CHECK_THAT(bumped.rho, WithinAbs(exact.rho, 1e-6));
  }
}

TEST_CASE("Call and put Greeks satisfy put-call parity", "[greeks][ch19]") {
  const Greeks c = BsmGreeks(OptionType::kCall, 40.0, kInputs);
  const Greeks p = BsmGreeks(OptionType::kPut, 40.0, kInputs);
  CHECK_THAT(c.delta - p.delta, WithinAbs(std::exp(-0.03 * 0.75), 1e-14));
  CHECK_THAT(c.gamma, WithinAbs(p.gamma, 1e-14));
  CHECK_THAT(c.vega, WithinAbs(p.vega, 1e-12));
  CHECK_THAT(c.rho - p.rho,
             WithinAbs(40.0 * 0.75 * std::exp(-0.08 * 0.75), 1e-12));
}

TEST_CASE("BSM satisfies its PDE through the Greeks", "[greeks][ch19]") {
  // Theta + (r - q) S Delta + sigma^2 S^2 Gamma / 2 = r V (eq. 19.4).
  const Greeks g = BsmGreeks(OptionType::kCall, 40.0, kInputs);
  const double s = kInputs.spot;
  const double v = BsmPricer(OptionType::kCall, 40.0)(kInputs);
  CHECK_THAT(
      g.theta + (kInputs.rate - kInputs.yield) * s * g.delta +
          0.5 * kInputs.volatility * kInputs.volatility * s * s * g.gamma,
      WithinAbs(kInputs.rate * v, 1e-12));
}

TEST_CASE("Short-dated theta falls back to a one-sided bump",
          "[greeks][ch19]") {
  OptionInputs in = kInputs;
  in.maturity = 0.5 / 365.0;
  const Greeks g = BumpGreeksErased(BsmPricer(OptionType::kCall, 42.0), in);
  CHECK(std::isfinite(g.theta));
  CHECK(g.theta < 0.0);
}

TEST_CASE("Neutral hedges zero the targeted Greeks", "[greeks][ch19]") {
  const Exposure portfolio{.delta = 300.0, .gamma = -1200.0, .vega = 900.0};
  const Exposure o1{.delta = 0.55, .gamma = 0.04, .vega = 0.12};
  const Exposure o2{.delta = 0.30, .gamma = 0.02, .vega = 0.15};
  const auto h = GammaVegaNeutralHedge(portfolio, o1, o2);
  REQUIRE(h.has_value());
  CHECK_THAT(portfolio.gamma + h->option1 * o1.gamma + h->option2 * o2.gamma,
             WithinAbs(0.0, 1e-9));
  CHECK_THAT(portfolio.vega + h->option1 * o1.vega + h->option2 * o2.vega,
             WithinAbs(0.0, 1e-9));
  CHECK_THAT(portfolio.delta + h->option1 * o1.delta + h->option2 * o2.delta +
                 h->underlying,
             WithinAbs(0.0, 1e-9));
  CHECK_FALSE(GammaVegaNeutralHedge(portfolio, o1, o1).has_value());
  CHECK_FALSE(GammaNeutralHedge(portfolio, {.delta = 1.0}).has_value());
}

TEST_CASE("Scenario grid is zero at the base point", "[greeks][ch19]") {
  const std::vector<double> spots = {40.0, 42.0, 44.0};
  const std::vector<double> vols = {0.20, 0.25, 0.30};
  const auto grid =
      ScenarioGrid(BsmPricer(OptionType::kCall, 40.0), kInputs, spots, vols);
  REQUIRE(grid.size() == 3);
  REQUIRE(grid[0].size() == 3);
  CHECK_THAT(grid[1][1], WithinAbs(0.0, 1e-14));
  CHECK(grid[2][2] > grid[1][1]);
  CHECK(grid[0][0] < 0.0);
}

TEST_CASE("Delta hedging costs the BSM price on average", "[greeks][ch19]") {
  const HedgeExperiment e{.spot = 49.0,
                          .strike = 50.0,
                          .rate = 0.05,
                          .drift = 0.13,
                          .volatility = 0.20,
                          .maturity = 20.0 / 52.0};
  const auto coarse = SimulateDeltaHedge(e, 5, 20000, 3);
  const auto fine = SimulateDeltaHedge(e, 50, 20000, 3);
  REQUIRE(coarse.has_value());
  REQUIRE(fine.has_value());
  const double se = fine->std_cost / std::sqrt(20000.0);
  CHECK_THAT(fine->mean_cost, WithinAbs(fine->bsm_price, 4.0 * se));
  CHECK(fine->ratio < coarse->ratio);
  // The hedging error shrinks like 1 / sqrt(rebalances).
  CHECK_THAT(coarse->ratio / fine->ratio, WithinRel(std::sqrt(10.0), 0.1));
  CHECK_FALSE(SimulateDeltaHedge(e, 0, 100, 1).has_value());
  CHECK_FALSE(SimulateDeltaHedge(e, 5, 1, 1).has_value());
}

}  // namespace
}  // namespace ofod::greeks
