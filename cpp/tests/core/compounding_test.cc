// Copyright 2026 Denis Zhereshchin

#include "ofod/core/compounding.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace ofod {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("ImpliedRate inverts GrowthFactor", "[core][ch04]") {
  const Compounding c =
      GENERATE(Compounding::Continuous(), Compounding::Periodic(1),
               Compounding::Periodic(4), Compounding::Simple());
  const double t = GENERATE(0.25, 1.0, 7.5);
  const double rate = 0.0437;
  CHECK_THAT(ImpliedRate(GrowthFactor(rate, t, c), t, c),
             WithinAbs(rate, 1e-14));
}

TEST_CASE("Periodic compounding converges to continuous", "[core][ch04]") {
  const double continuous = GrowthFactor(0.1, 1.0, Compounding::Continuous());
  const double daily = GrowthFactor(0.1, 1.0, Compounding::Periodic(365));
  CHECK_THAT(daily, WithinAbs(continuous, 2e-5));
  CHECK(daily < continuous);
}

TEST_CASE("EquivalentRate round trips", "[core][ch04]") {
  const auto semi = Compounding::Periodic(2);
  const auto cont = Compounding::Continuous();
  const double rc = EquivalentRate(0.1, semi, cont);
  CHECK_THAT(EquivalentRate(rc, cont, semi), WithinAbs(0.1, 1e-15));
}

TEST_CASE("DiscountFactor is the reciprocal of GrowthFactor", "[core]") {
  const auto c = Compounding::Periodic(12);
  CHECK_THAT(DiscountFactor(0.05, 3.0, c) * GrowthFactor(0.05, 3.0, c),
             WithinAbs(1.0, 1e-15));
}

}  // namespace
}  // namespace ofod
