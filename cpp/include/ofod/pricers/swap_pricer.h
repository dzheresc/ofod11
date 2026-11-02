// Copyright 2026 Denis Zhereshchin
//
// Swap valuation (Hull, Ch 7.5-7.9).

#ifndef OFOD_PRICERS_SWAP_PRICER_H_
#define OFOD_PRICERS_SWAP_PRICER_H_

#include <span>

#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/payoff.h"
#include "ofod/instruments/swap.h"

namespace ofod::pricers {

// Value of the fixed leg as a bond: coupons plus notional at maturity.
[[nodiscard]] double FixedLegBondValue(const instruments::InterestRateSwap& s,
                                       const curves::ZeroCurve& curve);

// Value of the floating leg as a floating-rate bond, worth par just after a
// reset: (notional + next floating payment) discounted to the next payment.
[[nodiscard]] double FloatingLegBondValue(
    const instruments::InterestRateSwap& s, const curves::ZeroCurve& curve);

// Swap value as the difference of two bonds. kLong receives fixed.
[[nodiscard]] double SwapValue(const instruments::InterestRateSwap& s,
                               const curves::ZeroCurve& curve,
                               instruments::Side side);

// Swap value as a portfolio of FRAs, assuming forward rates are realised.
// Equal to SwapValue on the same curve.
[[nodiscard]] double SwapValueAsFras(const instruments::InterestRateSwap& s,
                                     const curves::ZeroCurve& curve,
                                     instruments::Side side);

// Fixed rate that makes a new swap with these payment times worth zero.
[[nodiscard]] double ParSwapRate(std::span<const double> payment_times,
                                 int frequency, const curves::ZeroCurve& curve);

// Value of a currency swap in domestic currency to the party that receives
// the foreign leg (kLong) given spot = domestic units per foreign unit.
[[nodiscard]] double CurrencySwapValue(const instruments::CurrencySwap& s,
                                       const curves::ZeroCurve& domestic,
                                       const curves::ZeroCurve& foreign,
                                       double spot, instruments::Side side);

// Total gain available to two parties from the comparative-advantage
// argument (Ch 7.5): the difference between the fixed-rate spread and the
// floating-rate spread.
[[nodiscard]] double ComparativeAdvantageGain(double fixed_a, double fixed_b,
                                              double floating_spread_a,
                                              double floating_spread_b);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_SWAP_PRICER_H_
