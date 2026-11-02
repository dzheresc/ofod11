// Copyright 2026 Denis Zhereshchin

#include "ofod/curves/zero_curve.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

namespace ofod::curves {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("ZeroCurve discounting and interpolation", "[curves][ch04]") {
  const auto curve = ZeroCurve::Create({0.5, 1.0}, {0.05, 0.058});
  REQUIRE(curve.has_value());
  CHECK_THAT(curve->ZeroRate(0.75), WithinAbs(0.054, 1e-15));
  CHECK_THAT(curve->ZeroRate(0.1), WithinAbs(0.05, 1e-15));
  CHECK_THAT(curve->DiscountFactor(1.0), WithinAbs(std::exp(-0.058), 1e-15));
}

TEST_CASE("Forward rates reproduce the long zero rate", "[curves][ch04]") {
  const auto curve = ZeroCurve::Create({1, 2, 3}, {0.03, 0.04, 0.046});
  REQUIRE(curve.has_value());
  // Investing at R1 then at F(1,2) equals investing at R2.
  const double f12 = curve->ForwardRate(1.0, 2.0);
  CHECK_THAT(0.03 * 1.0 + f12 * 1.0, WithinAbs(0.04 * 2.0, 1e-15));
}

TEST_CASE("Flat curve", "[curves]") {
  const ZeroCurve curve = ZeroCurve::Flat(0.07);
  CHECK_THAT(curve.ZeroRate(30.0), WithinAbs(0.07, 1e-15));
  CHECK_THAT(curve.ForwardRate(2.0, 5.0), WithinAbs(0.07, 1e-15));
}

}  // namespace
}  // namespace ofod::curves
