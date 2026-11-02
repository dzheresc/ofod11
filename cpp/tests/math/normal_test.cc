// Copyright 2026 Denis Zhereshchin

#include "ofod/math/normal.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace ofod::math {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("NormCdf matches tabulated values", "[math][ch15]") {
  // Values from Hull's N(x) table at the end of the book.
  CHECK_THAT(NormCdf(0.0), WithinAbs(0.5, 1e-15));
  CHECK_THAT(NormCdf(0.6278), WithinAbs(0.7349, 1e-4));
  CHECK_THAT(NormCdf(-0.12), WithinAbs(0.4522, 1e-4));
  CHECK_THAT(NormCdf(1.96), WithinAbs(0.9750, 1e-4));
}

TEST_CASE("NormCdfPoly agrees with erfc", "[math][ch15]") {
  for (int i = -600; i <= 600; ++i) {
    const double x = 0.01 * i;
    CHECK_THAT(NormCdfPoly(x), WithinAbs(NormCdf(x), 7.5e-8));
  }
}

TEST_CASE("NormCdf is symmetric", "[math]") {
  for (int i = 0; i <= 32; ++i) {
    const double x = 0.25 * i;
    CHECK_THAT(NormCdf(x) + NormCdf(-x), WithinAbs(1.0, 1e-15));
  }
}

}  // namespace
}  // namespace ofod::math
