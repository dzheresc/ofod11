// Copyright 2026 Denis Zhereshchin

#include "ofod/instruments/bond.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace ofod::instruments {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Semiannual coupon bond cash flows", "[instruments][ch04]") {
  const auto flows = CashFlows(Bond{.coupon_rate = 0.06, .maturity = 2.0});
  REQUIRE(flows.size() == 4);
  CHECK_THAT(flows.front().time, WithinAbs(0.5, 1e-12));
  CHECK_THAT(flows.front().amount, WithinAbs(3.0, 1e-12));
  CHECK_THAT(flows.back().time, WithinAbs(2.0, 1e-12));
  CHECK_THAT(flows.back().amount, WithinAbs(103.0, 1e-12));
}

TEST_CASE("Zero-coupon bond pays face only", "[instruments][ch04]") {
  const auto flows = CashFlows(Bond{.maturity = 0.25});
  REQUIRE(flows.size() == 1);
  CHECK_THAT(flows[0].time, WithinAbs(0.25, 1e-12));
  CHECK_THAT(flows[0].amount, WithinAbs(100.0, 1e-12));
}

TEST_CASE("Odd first period", "[instruments]") {
  // 1.25 years, semiannual: coupons at 0.25, 0.75, 1.25.
  const auto flows = CashFlows(Bond{.coupon_rate = 0.08, .maturity = 1.25});
  REQUIRE(flows.size() == 3);
  CHECK_THAT(flows[0].time, WithinAbs(0.25, 1e-12));
}

}  // namespace
}  // namespace ofod::instruments
