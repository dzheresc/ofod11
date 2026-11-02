// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/swap_pricer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>

#include "ofod/curves/swap_bootstrap.h"

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using instruments::InterestRateSwap;
using instruments::Side;

TEST_CASE("A swap at the par rate is worth zero", "[pricers][ch07]") {
  const auto curve =
      *curves::ZeroCurve::Create({0.5, 1, 2, 3}, {0.03, 0.032, 0.035, 0.037});
  const std::vector<double> times = {0.5, 1.0, 1.5, 2.0, 2.5, 3.0};
  const double par = ParSwapRate(times, 2, curve);
  // Just after a reset the floating rate equals the 6-month rate.
  const double next = (1.0 / curve.DiscountFactor(0.5) - 1.0) / 0.5;
  const InterestRateSwap swap{.notional = 1e6,
                              .fixed_rate = par,
                              .frequency = 2,
                              .payment_times = times,
                              .next_floating_rate = next};
  CHECK_THAT(SwapValue(swap, curve, Side::kLong), WithinAbs(0.0, 1e-6));
  CHECK_THAT(SwapValueAsFras(swap, curve, Side::kShort), WithinAbs(0.0, 1e-6));
}

TEST_CASE("Bond and FRA valuations agree", "[pricers][ch07]") {
  const auto curve = *curves::ZeroCurve::Create({0.25, 1.25}, {0.02, 0.05});
  const InterestRateSwap swap{.notional = 100,
                              .fixed_rate = 0.04,
                              .frequency = 4,
                              .payment_times = {0.25, 0.5, 0.75, 1.0, 1.25},
                              .next_floating_rate = 0.03};
  CHECK_THAT(SwapValueAsFras(swap, curve, Side::kLong),
             WithinAbs(SwapValue(swap, curve, Side::kLong), 1e-12));
}

TEST_CASE("Swap bootstrap reprices par swaps", "[curves][ch07]") {
  const std::vector<curves::SwapQuote> quotes = {
      {1.0, 0.030, 1}, {2.0, 0.034, 1}, {3.0, 0.037, 1}, {5.0, 0.040, 1}};
  const auto curve = curves::BootstrapFromSwaps(quotes);
  REQUIRE(curve.has_value());
  CHECK_THAT(ParSwapRate(std::vector<double>{1, 2, 3}, 1, *curve),
             WithinAbs(0.037, 1e-10));
  CHECK_THAT(ParSwapRate(std::vector<double>{1, 2, 3, 4, 5}, 1, *curve),
             WithinAbs(0.040, 1e-10));
}

}  // namespace
}  // namespace ofod::pricers
