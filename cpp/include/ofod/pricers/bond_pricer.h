// Copyright 2026 Denis Zhereshchin
//
// Bond pricing, yield, duration and convexity (Hull, Ch 4.6, 4.10, 4.11).

#ifndef OFOD_PRICERS_BOND_PRICER_H_
#define OFOD_PRICERS_BOND_PRICER_H_

#include <optional>

#include "ofod/core/compounding.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/bond.h"

namespace ofod::pricers {

// Price of `bond` discounted on `curve`.
[[nodiscard]] double BondPrice(const instruments::Bond& bond,
                               const curves::ZeroCurve& curve);

// Price of `bond` discounted at a single yield under `compounding`.
[[nodiscard]] double BondPriceFromYield(
    const instruments::Bond& bond, double yield,
    Compounding compounding = Compounding::Continuous());

// Yield (under `compounding`) that reprices `bond` to `price`.
[[nodiscard]] std::optional<double> BondYield(
    const instruments::Bond& bond, double price,
    Compounding compounding = Compounding::Continuous());

// Macaulay duration: time-weighted average of the discounted cash flows.
// With a continuously compounded yield this is also -(dB/dy)/B.
[[nodiscard]] double MacaulayDuration(
    const instruments::Bond& bond, double yield,
    Compounding compounding = Compounding::Continuous());

// Modified duration D / (1 + y/m) for a yield compounded m times per year.
// For a continuous yield it equals the Macaulay duration.
[[nodiscard]] double ModifiedDuration(const instruments::Bond& bond,
                                      double yield, Compounding compounding);

// Convexity (1/B) d2B/dy2 for a continuously compounded yield.
[[nodiscard]] double Convexity(const instruments::Bond& bond, double yield);

// Coupon rate that makes a bond with `maturity` and `frequency` price at par
// on `curve`.
[[nodiscard]] double ParYield(double maturity, int frequency,
                              const curves::ZeroCurve& curve);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_BOND_PRICER_H_
