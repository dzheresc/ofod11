// Copyright 2026 Denis Zhereshchin

#include "ofod/instruments/option.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ofod/instruments/payoff.h"
#include "ofod/instruments/portfolio.h"

namespace ofod::instruments {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Intrinsic value of calls and puts", "[instruments][ch10]") {
  const VanillaOption call{.option_type = OptionType::kCall, .strike = 50.0};
  const VanillaOption put{.option_type = OptionType::kPut, .strike = 50.0};
  CHECK(IntrinsicValue(call, 60.0) == 10.0);
  CHECK(IntrinsicValue(call, 40.0) == 0.0);
  CHECK(IntrinsicValue(put, 40.0) == 10.0);
  CHECK(IntrinsicValue(put, 60.0) == 0.0);
}

TEST_CASE("Naked put margin uses 10% of the strike floor",
          "[instruments][ch10]") {
  // Deep out of the money: premium + 0.1 K dominates.
  CHECK_THAT(NakedOptionMargin(OptionType::kPut, 1.0, 100.0, 60.0),
             WithinAbs(7.0, 1e-12));
  // At the money: premium + 0.2 S.
  CHECK_THAT(NakedOptionMargin(OptionType::kPut, 2.0, 50.0, 50.0),
             WithinAbs(12.0, 1e-12));
}

TEST_CASE("Portfolio sums its legs", "[instruments][ch12]") {
  Portfolio p;
  p.Add(1.0, StockPayoff{}, 40.0).Add(-1.0, CallPayoff{45.0}, 2.0);
  CHECK(p.Cost() == 38.0);
  CHECK(p.PayoffAt(50.0) == 45.0);
  CHECK(p.ProfitAt(30.0) == -8.0);
  CHECK(p.legs().size() == 2);
}

}  // namespace
}  // namespace ofod::instruments
