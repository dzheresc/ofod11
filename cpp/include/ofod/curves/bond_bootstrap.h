// Copyright 2026 Denis Zhereshchin
//
// Zero curve bootstrapping from bond prices (Hull, Ch 4.7).

#ifndef OFOD_CURVES_BOND_BOOTSTRAP_H_
#define OFOD_CURVES_BOND_BOOTSTRAP_H_

#include <optional>
#include <span>

#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/bond.h"

namespace ofod::curves {

struct BondQuote {
  instruments::Bond bond;
  double price = 0.0;
};

// Builds a zero curve with one pillar per bond maturity. Bonds are processed
// in maturity order; coupons that fall between known pillars use the
// curve's linear interpolation, and coupons between the last known pillar
// and the new maturity interpolate towards the unknown rate, which is solved
// so the bond reprices exactly. Returns nullopt if a bond cannot be repriced
// or two bonds share a maturity.
[[nodiscard]] std::optional<ZeroCurve> BootstrapFromBonds(
    std::span<const BondQuote> quotes);

}  // namespace ofod::curves

#endif  // OFOD_CURVES_BOND_BOOTSTRAP_H_
