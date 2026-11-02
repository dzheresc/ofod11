// Copyright 2026 Denis Zhereshchin
//
// FRA valuation (Hull, Ch 4.9).

#ifndef OFOD_PRICERS_FRA_PRICER_H_
#define OFOD_PRICERS_FRA_PRICER_H_

#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/fra.h"
#include "ofod/instruments/payoff.h"

namespace ofod::pricers {

// Forward rate for [t1, t2] with simple compounding over the period.
[[nodiscard]] double SimpleForwardRate(const curves::ZeroCurve& curve,
                                       double t1, double t2);

// Value of `fra` today. kLong receives the fixed rate and pays floating;
// kShort pays fixed. Assumes the forward rate is realised and discounts the
// payment at `end` on `curve`.
[[nodiscard]] double FraValue(const instruments::Fra& fra,
                              const curves::ZeroCurve& curve,
                              instruments::Side side);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_FRA_PRICER_H_
