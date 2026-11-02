// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/bond_pricer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using instruments::Bond;

constexpr Bond kBond{.coupon_rate = 0.10, .maturity = 3.0};

TEST_CASE("Yield inverts price", "[pricers][ch04]") {
  for (const auto c : {Compounding::Continuous(), Compounding::Periodic(2)}) {
    const double price = BondPriceFromYield(kBond, 0.07, c);
    const auto y = BondYield(kBond, price, c);
    REQUIRE(y.has_value());
    CHECK_THAT(*y, WithinAbs(0.07, 1e-10));
  }
}

TEST_CASE("Flat curve price equals yield price", "[pricers][ch04]") {
  const auto curve = curves::ZeroCurve::Flat(0.05);
  CHECK_THAT(BondPrice(kBond, curve),
             WithinAbs(BondPriceFromYield(kBond, 0.05), 1e-12));
}

TEST_CASE("Duration and convexity match finite differences", "[ch04]") {
  const double y = 0.12;
  const double h = 1e-4;
  const double b0 = BondPriceFromYield(kBond, y);
  const double up = BondPriceFromYield(kBond, y + h);
  const double down = BondPriceFromYield(kBond, y - h);
  const double duration = -(up - down) / (2.0 * h) / b0;
  const double convexity = (up - 2.0 * b0 + down) / (h * h) / b0;
  CHECK_THAT(MacaulayDuration(kBond, y), WithinAbs(duration, 1e-6));
  CHECK_THAT(Convexity(kBond, y), WithinAbs(convexity, 1e-4));
}

TEST_CASE("Modified duration is the periodic-yield sensitivity", "[ch04]") {
  const auto semi = Compounding::Periodic(2);
  const double y = 0.1236731;
  const double h = 1e-5;
  const double b0 = BondPriceFromYield(kBond, y, semi);
  const double slope = (BondPriceFromYield(kBond, y + h, semi) -
                        BondPriceFromYield(kBond, y - h, semi)) /
                       (2.0 * h);
  CHECK_THAT(ModifiedDuration(kBond, y, semi), WithinAbs(-slope / b0, 1e-7));
}

TEST_CASE("Par yield bond prices at par", "[pricers][ch04]") {
  const auto curve = curves::ZeroCurve::Create({0.5, 1.0, 1.5, 2.0},
                                               {0.05, 0.058, 0.064, 0.068});
  REQUIRE(curve.has_value());
  const double c = ParYield(2.0, 2, *curve);
  CHECK_THAT(BondPrice(Bond{.coupon_rate = c, .maturity = 2.0}, *curve),
             WithinAbs(100.0, 1e-10));
}

}  // namespace
}  // namespace ofod::pricers
