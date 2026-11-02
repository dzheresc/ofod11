// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/bounds.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Parity round-trips", "[bounds][ch11]") {
  const double p = PutFromParity(4.0, 50.0, 48.0, 0.05, 0.75, 1.2);
  CHECK_THAT(CallFromParity(p, 50.0, 48.0, 0.05, 0.75, 1.2),
             WithinAbs(4.0, 1e-12));
}

TEST_CASE("Bounds are ordered", "[bounds][ch11]") {
  CHECK(EuropeanCallLowerBound(40.0, 50.0, 0.05, 1.0) == 0.0);
  CHECK(EuropeanPutUpperBound(50.0, 0.05, 1.0) < AmericanPutUpperBound(50.0));
  CHECK(CallUpperBound(40.0) == 40.0);
  const auto r = AmericanCallMinusPut(40.0, 40.0, 0.05, 1.0);
  CHECK(r.lower < r.upper);
}

TEST_CASE("Early exercise test for dividends", "[bounds][ch11]") {
  // K (1 - e^{-r tau}) = 40 (1 - e^{-0.09 * 0.25}) = 0.89.
  CHECK_FALSE(CallEarlyExerciseMayBeOptimal(0.5, 40.0, 0.09, 0.25));
  CHECK(CallEarlyExerciseMayBeOptimal(1.0, 40.0, 0.09, 0.25));
}

}  // namespace
}  // namespace ofod::pricers
