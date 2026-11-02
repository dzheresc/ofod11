// Copyright 2026 Denis Zhereshchin
//
// Fixed-coupon bullet bond (Hull, Ch 4.6).

#ifndef OFOD_INSTRUMENTS_BOND_H_
#define OFOD_INSTRUMENTS_BOND_H_

#include <vector>

#include "ofod/instruments/cash_flow.h"

namespace ofod::instruments {

struct Bond {
  double face = 100.0;
  double coupon_rate = 0.0;  // Annual coupon as a fraction of face.
  double maturity = 0.0;     // Years from today.
  int frequency = 2;         // Coupons per year.
};

// Returns the bond's remaining cash flows in increasing time order. Coupons
// fall at maturity - k / frequency; a zero-coupon bond pays only its face.
[[nodiscard]] std::vector<CashFlow> CashFlows(const Bond& bond);

}  // namespace ofod::instruments

#endif  // OFOD_INSTRUMENTS_BOND_H_
