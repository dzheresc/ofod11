// Copyright 2026 Denis Zhereshchin

#include "ofod/math/roots.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

namespace ofod::math {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Brent finds roots", "[math]") {
  const auto root = Brent([](double x) { return x * x - 2.0; }, 0.0, 2.0);
  REQUIRE(root.has_value());
  CHECK_THAT(*root, WithinAbs(std::sqrt(2.0), 1e-12));

  const auto cosine = Brent([](double x) { return std::cos(x) - x; }, 0, 1);
  REQUIRE(cosine.has_value());
  CHECK_THAT(std::cos(*cosine), WithinAbs(*cosine, 1e-12));
}

TEST_CASE("Brent rejects an unbracketed interval", "[math]") {
  CHECK_FALSE(Brent([](double x) { return x * x + 1.0; }, -1.0, 1.0));
}

TEST_CASE("Newton converges quadratically to sqrt(2)", "[math]") {
  const auto root = Newton([](double x) { return x * x - 2.0; },
                           [](double x) { return 2.0 * x; }, 1.0);
  REQUIRE(root.has_value());
  CHECK_THAT(*root, WithinAbs(std::sqrt(2.0), 1e-14));
}

TEST_CASE("Newton fails on a zero derivative", "[math]") {
  CHECK_FALSE(Newton([](double x) { return x * x + 1.0; },
                     [](double x) { return 2.0 * x; }, 0.0));
}

}  // namespace
}  // namespace ofod::math
