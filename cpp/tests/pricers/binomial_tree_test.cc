// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/binomial_tree.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "ofod/math/normal.h"
#include "ofod/pricers/bounds.h"

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using instruments::Exercise;
using instruments::OptionType;
using instruments::VanillaOption;

// Black-Scholes-Merton, used only as the convergence target here.
double Bsm(OptionType type, double s, double k, double r, double q,
           double sigma, double t) {
  const double d1 = (std::log(s / k) + (r - q + 0.5 * sigma * sigma) * t) /
                    (sigma * std::sqrt(t));
  const double d2 = d1 - sigma * std::sqrt(t);
  if (type == OptionType::kCall) {
    return s * std::exp(-q * t) * math::NormCdf(d1) -
           k * std::exp(-r * t) * math::NormCdf(d2);
  }
  return k * std::exp(-r * t) * math::NormCdf(-d2) -
         s * std::exp(-q * t) * math::NormCdf(-d1);
}

TEST_CASE("All schemes converge to BSM", "[lattice][ch13]") {
  const VanillaOption call{.strike = 40.0, .maturity = 0.5};
  const TreeMarket m{
      .spot = 42.0, .rate = 0.10, .yield = 0.03, .volatility = 0.20};
  const double target =
      Bsm(OptionType::kCall, 42.0, 40.0, 0.10, 0.03, 0.2, 0.5);
  for (auto scheme : {TreeScheme::kCoxRossRubinstein, TreeScheme::kJarrowRudd,
                      TreeScheme::kTian}) {
    const auto r = PriceBinomial(call, m, 1000, scheme);
    REQUIRE(r.has_value());
    CHECK_THAT(r->price, WithinAbs(target, 2e-3));
  }
}

TEST_CASE("Template and runtime dispatch agree", "[lattice][ch13]") {
  const VanillaOption put{.option_type = OptionType::kPut,
                          .exercise = Exercise::kAmerican,
                          .strike = 50.0,
                          .maturity = 1.0};
  const TreeMarket m{.spot = 48.0, .rate = 0.05, .volatility = 0.3};
  const auto a = PriceBinomial<Tian>(put, m, 101);
  const auto b = PriceBinomial(put, m, 101, TreeScheme::kTian);
  REQUIRE(a.has_value());
  REQUIRE(b.has_value());
  CHECK(a->price == b->price);
}

TEST_CASE("American put is at least European and intrinsic",
          "[lattice][ch13]") {
  VanillaOption put{
      .option_type = OptionType::kPut, .strike = 50.0, .maturity = 2.0};
  const TreeMarket m{.spot = 40.0, .rate = 0.08, .volatility = 0.25};
  const auto european = PriceBinomial<CoxRossRubinstein>(put, m, 200);
  put.exercise = Exercise::kAmerican;
  const auto american = PriceBinomial<CoxRossRubinstein>(put, m, 200);
  REQUIRE(european.has_value());
  REQUIRE(american.has_value());
  CHECK(american->price > european->price);
  CHECK(american->price >= 10.0);
  CHECK(european->price >= EuropeanPutLowerBound(40.0, 50.0, 0.08, 2.0));
}

TEST_CASE("Futures options use q = r", "[lattice][ch13]") {
  // With q = r the up probability is (1 - d) / (u - d) (eq. 13.17).
  const TreeStep step = CoxRossRubinstein::Step(0.3, 0.0, 0.25);
  CHECK_THAT(step.probability,
             WithinAbs((1.0 - step.down) / (step.up - step.down), 1e-15));
}

TEST_CASE("Invalid trees are rejected", "[lattice][ch13]") {
  const VanillaOption call{.strike = 21.0, .maturity = 0.25};
  CHECK_FALSE(PriceTree(call, 20.0, 0.12, 0, {1.1, 0.9, 0.6}));
  CHECK_FALSE(PriceTree(call, 20.0, 0.12, 1, {1.1, 0.9, 1.2}));
  CHECK_FALSE(PriceTree(call, 20.0, 0.12, 1, {0.9, 1.1, 0.5}));
}

}  // namespace
}  // namespace ofod::pricers
