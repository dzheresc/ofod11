// Copyright 2026 Denis Zhereshchin

#include "ofod/curves/bond_bootstrap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>

#include "ofod/pricers/bond_pricer.h"

namespace ofod::curves {
namespace {

using Catch::Matchers::WithinAbs;
using instruments::Bond;

TEST_CASE("Bootstrapped curve reprices every input bond", "[curves][ch04]") {
  const std::vector<BondQuote> quotes = {
      {Bond{.coupon_rate = 0.05, .maturity = 2.0}, 99.0},
      {Bond{.coupon_rate = 0.00, .maturity = 0.5}, 98.0},
      {Bond{.coupon_rate = 0.04, .maturity = 1.0}, 99.5},
      {Bond{.coupon_rate = 0.06, .maturity = 1.5}, 101.0},
  };
  const auto curve = BootstrapFromBonds(quotes);
  REQUIRE(curve.has_value());
  CHECK(curve->times().size() == 4);
  for (const auto& quote : quotes) {
    CHECK_THAT(pricers::BondPrice(quote.bond, *curve),
               WithinAbs(quote.price, 1e-9));
  }
}

TEST_CASE("Bootstrap rejects duplicate maturities", "[curves]") {
  const std::vector<BondQuote> quotes = {
      {Bond{.maturity = 1.0}, 95.0},
      {Bond{.coupon_rate = 0.05, .maturity = 1.0}, 100.0},
  };
  CHECK_FALSE(BootstrapFromBonds(quotes));
}

}  // namespace
}  // namespace ofod::curves
