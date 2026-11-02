// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/fra_pricer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using instruments::Fra;
using instruments::Side;

TEST_CASE("FRA at the forward rate is worth zero", "[pricers][ch04]") {
  const auto curve = curves::ZeroCurve::Create({1, 2}, {0.03, 0.04});
  REQUIRE(curve.has_value());
  const double forward = SimpleForwardRate(*curve, 1.0, 2.0);
  CHECK_THAT(forward, WithinAbs(std::exp(0.05) - 1.0, 1e-14));
  const Fra fra{.notional = 1e8, .fixed_rate = forward, .start = 1, .end = 2};
  CHECK_THAT(FraValue(fra, *curve, Side::kLong), WithinAbs(0.0, 1e-6));
}

TEST_CASE("FRA value is linear in the fixed rate", "[pricers][ch04]") {
  const auto curve = curves::ZeroCurve::Flat(0.04);
  const double forward = SimpleForwardRate(curve, 1.0, 1.5);
  const Fra fra{
      .notional = 1e6, .fixed_rate = forward + 0.01, .start = 1.0, .end = 1.5};
  const double expected = 1e6 * 0.01 * 0.5 * std::exp(-0.04 * 1.5);
  CHECK_THAT(FraValue(fra, curve, Side::kLong), WithinAbs(expected, 1e-8));
  CHECK_THAT(FraValue(fra, curve, Side::kShort), WithinAbs(-expected, 1e-8));
}

}  // namespace
}  // namespace ofod::pricers
