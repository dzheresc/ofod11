// Copyright 2026 Denis Zhereshchin

#include "ofod/math/interpolation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace ofod::math {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("LinearInterpolator interpolates and extrapolates flat", "[math]") {
  const auto f = LinearInterpolator::Create({1.0, 2.0, 4.0}, {10.0, 20.0, 0.0});
  REQUIRE(f.has_value());
  CHECK_THAT((*f)(1.5), WithinAbs(15.0, 1e-15));
  CHECK_THAT((*f)(3.0), WithinAbs(10.0, 1e-15));
  CHECK_THAT((*f)(2.0), WithinAbs(20.0, 1e-15));
  CHECK_THAT((*f)(0.0), WithinAbs(10.0, 1e-15));
  CHECK_THAT((*f)(9.0), WithinAbs(0.0, 1e-15));
}

TEST_CASE("LinearInterpolator validates input", "[math]") {
  CHECK_FALSE(LinearInterpolator::Create({}, {}));
  CHECK_FALSE(LinearInterpolator::Create({1.0, 2.0}, {1.0}));
  CHECK_FALSE(LinearInterpolator::Create({1.0, 1.0}, {1.0, 2.0}));
  CHECK_FALSE(LinearInterpolator::Create({2.0, 1.0}, {1.0, 2.0}));
}

}  // namespace
}  // namespace ofod::math
