// Copyright 2026 Denis Zhereshchin

#include "ofod/instruments/payoff.h"

#include <catch2/catch_test_macros.hpp>

namespace ofod::instruments {
namespace {

static_assert(Payoff<CallPayoff>);
static_assert(Payoff<PutPayoff>);
static_assert(Payoff<ForwardPayoff>);
static_assert(CallPayoff{100.0}(120.0) == 20.0);
static_assert(PutPayoff{100.0}(120.0) == 0.0);

TEST_CASE("Long and short forward payoffs mirror each other", "[ch01]") {
  const Position<ForwardPayoff> long_fwd{.payoff = {1.3}};
  const Position<ForwardPayoff> short_fwd{.payoff = {1.3},
                                          .side = Side::kShort};
  for (double s : {1.0, 1.3, 1.6}) {
    CHECK(long_fwd.PayoffAt(s) == -short_fwd.PayoffAt(s));
  }
}

TEST_CASE("Option profit nets the premium", "[ch01]") {
  // Long 100 calls, strike 340, premium 5: breakeven at 345.
  const Position<CallPayoff> calls{
      .payoff = {340.0}, .quantity = 100.0, .premium = 5.0};
  CHECK(calls.ProfitAt(345.0) == 0.0);
  CHECK(calls.ProfitAt(300.0) == -500.0);
  CHECK(calls.ProfitAt(400.0) == 5500.0);

  // Writing puts: the premium is kept when the put expires worthless.
  const Position<PutPayoff> written{.payoff = {90.0},
                                    .side = Side::kShort,
                                    .quantity = 100.0,
                                    .premium = 7.0};
  CHECK(written.ProfitAt(100.0) == 700.0);
  CHECK(written.ProfitAt(80.0) == -300.0);
}

}  // namespace
}  // namespace ofod::instruments
