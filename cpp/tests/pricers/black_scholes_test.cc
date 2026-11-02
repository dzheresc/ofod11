// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/black_scholes.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/bounds.h"

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using instruments::OptionType;

TEST_CASE("BSM satisfies put-call parity with a yield", "[bsm][ch17]") {
  const double c = BsmPrice(OptionType::kCall, 50, 45, 0.04, 0.02, 0.3, 1.5);
  const double p = BsmPrice(OptionType::kPut, 50, 45, 0.04, 0.02, 0.3, 1.5);
  CHECK_THAT(c - p,
             WithinAbs(50 * std::exp(-0.03) - 45 * std::exp(-0.06), 1e-12));
}

TEST_CASE("Implied volatility round-trips", "[bsm][ch15]") {
  for (double vol : {0.05, 0.2, 0.6, 1.5}) {
    for (auto type : {OptionType::kCall, OptionType::kPut}) {
      const double price = BsmPrice(type, 100, 110, 0.03, 0.01, vol, 0.75);
      const auto iv =
          ImpliedVolatility(type, price, 100, 110, 0.03, 0.01, 0.75);
      REQUIRE(iv.has_value());
      CHECK_THAT(*iv, WithinAbs(vol, 1e-9));
    }
  }
  CHECK_FALSE(ImpliedVolatility(OptionType::kCall, 101, 100, 90, 0.05, 0, 1));
  CHECK_FALSE(ImpliedVolatility(OptionType::kCall, 0.0, 100, 90, 0.05, 0, 1));
}

TEST_CASE("Black's model equals BSM on the forward", "[bsm][ch18]") {
  const double r = 0.05;
  const double t = 0.8;
  const double s = 40.0;
  const double f = s * std::exp(r * t);
  CHECK_THAT(
      Black76Price(OptionType::kCall, f, 42, r, 0.25, t),
      WithinAbs(BsmPrice(OptionType::kCall, s, 42, r, 0, 0.25, t), 1e-12));
  const double c = Black76Price(OptionType::kCall, f, 42, r, 0.25, t);
  CHECK_THAT(
      FuturesPutFromParity(c, f, 42, r, t),
      WithinAbs(Black76Price(OptionType::kPut, f, 42, r, 0.25, t), 1e-12));
  CHECK_THAT(FuturesStyleOptionPrice(OptionType::kCall, f, 42, 0.25, t),
             WithinAbs(c * std::exp(r * t), 1e-12));
}

TEST_CASE("Futures options on a tree converge to Black", "[bsm][ch18]") {
  const instruments::VanillaOption put{
      .option_type = OptionType::kPut, .strike = 20, .maturity = 1.0 / 3.0};
  const TreeMarket m{
      .spot = 20, .rate = 0.09, .yield = 0.09, .volatility = 0.25};
  const auto tree = PriceBinomial<CoxRossRubinstein>(put, m, 2000);
  REQUIRE(tree.has_value());
  CHECK_THAT(tree->price, WithinAbs(Black76Price(OptionType::kPut, 20, 20, 0.09,
                                                 0.25, 1.0 / 3.0),
                                    1e-3));
}

TEST_CASE("BSM prices respect the Ch 11 bounds", "[bsm][ch11]") {
  for (double k : {20.0, 40.0, 60.0}) {
    const double c = BsmPrice(OptionType::kCall, 40, k, 0.05, 0, 0.3, 2);
    const double p = BsmPrice(OptionType::kPut, 40, k, 0.05, 0, 0.3, 2);
    CHECK(c >= EuropeanCallLowerBound(40, k, 0.05, 2));
    CHECK(c <= CallUpperBound(40));
    CHECK(p >= EuropeanPutLowerBound(40, k, 0.05, 2));
    CHECK(p <= EuropeanPutUpperBound(k, 0.05, 2));
  }
}

TEST_CASE("No dividends: Black's approximation is the European call",
          "[bsm][ch15]") {
  const std::vector<instruments::CashFlow> none;
  CHECK(BlackAmericanCallApproximation(40, 40, 0.09, 0.3, 0.5, none) ==
        BsmPrice(OptionType::kCall, 40, 40, 0.09, 0, 0.3, 0.5));
}

}  // namespace
}  // namespace ofod::pricers
