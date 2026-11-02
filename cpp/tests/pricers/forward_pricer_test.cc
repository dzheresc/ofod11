// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/forward_pricer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using instruments::Side;

TEST_CASE("Forward formulas agree in special cases", "[pricers][ch05]") {
  const double f = ForwardPrice(100.0, 0.05, 2.0);
  CHECK_THAT(ForwardPriceWithIncome(100.0, 0.0, 0.05, 2.0),
             WithinAbs(f, 1e-12));
  CHECK_THAT(ForwardPriceWithYield(100.0, 0.05, 0.0, 2.0), WithinAbs(f, 1e-12));
  CHECK_THAT(ForwardPriceCostOfCarry(100.0, 0.05, 0.0, 0.0, 2.0),
             WithinAbs(f, 1e-12));
}

TEST_CASE("Currency forward follows interest rate parity", "[ch05]") {
  // F = S exp((r - rf) T): a higher foreign rate gives a forward discount.
  const double f = ForwardPriceWithYield(0.75, 0.01, 0.03, 2.0);
  CHECK(f < 0.75);
  CHECK_THAT(f, WithinAbs(0.75 * std::exp(-0.04), 1e-15));
}

TEST_CASE("A new forward at the market price is worth zero", "[ch05]") {
  const double f = ForwardPrice(25.0, 0.1, 0.5);
  CHECK_THAT(ForwardContractValue(f, f, 0.1, 0.5, Side::kLong),
             WithinAbs(0.0, 1e-15));
  CHECK(ForwardContractValue(f, 24.0, 0.1, 0.5, Side::kLong) ==
        -ForwardContractValue(f, 24.0, 0.1, 0.5, Side::kShort));
}

TEST_CASE("Arbitrage direction", "[ch05]") {
  // Section 5.4: spot 40, r 5%, 3 months gives 40.50.
  const double theory = ForwardPrice(40.0, 0.05, 0.25);
  CHECK(DetectForwardArbitrage(43.0, theory) ==
        ForwardArbitrage::kCashAndCarry);
  CHECK(DetectForwardArbitrage(39.0, theory) ==
        ForwardArbitrage::kReverseCashAndCarry);
  CHECK(DetectForwardArbitrage(theory, theory) == ForwardArbitrage::kNone);
}

}  // namespace
}  // namespace ofod::pricers
