// Copyright 2026 Denis Zhereshchin

#include "ofod/risk/hedging.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>

namespace ofod::risk {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Regression hedge ratio recovers a linear relation", "[ch03]") {
  const std::vector<double> df = {0.021, -0.012, 0.035, -0.008, 0.014, 0.0};
  std::vector<double> ds;
  for (double x : df) ds.push_back(0.8 * x + 0.001);
  const auto h = HedgeRatioFromChanges(ds, df);
  REQUIRE(h.has_value());
  CHECK_THAT(*h, WithinAbs(0.8, 1e-12));
  CHECK_FALSE(HedgeRatioFromChanges(ds, std::vector<double>{1.0}));
}

TEST_CASE("Hedge price and basis", "[ch03]") {
  // Example 3.3-style short hedge: F1 = 2.20, F2 = 2.00, S2 = 1.90.
  CHECK_THAT(EffectiveHedgePrice(1.90, 2.20, 2.00), WithinAbs(2.10, 1e-12));
  CHECK_THAT(Basis(1.90, 2.00), WithinAbs(-0.10, 1e-12));
  CHECK_THAT(HedgeEffectiveness(0.928), WithinAbs(0.861184, 1e-12));
}

TEST_CASE("Stack and roll gain", "[ch03]") {
  const std::vector<RollLeg> legs = {{20.0, 19.1}, {19.0, 18.3}, {18.2, 17.2}};
  CHECK_THAT(ShortRollGain(legs), WithinAbs(2.6, 1e-12));
}

}  // namespace
}  // namespace ofod::risk
