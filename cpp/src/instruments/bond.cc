// Copyright 2026 Denis Zhereshchin

#include "ofod/instruments/bond.h"

#include <algorithm>

namespace ofod::instruments {

std::vector<CashFlow> CashFlows(const Bond& bond) {
  constexpr double kEpsilon = 1e-9;
  std::vector<CashFlow> flows;
  const double coupon = bond.face * bond.coupon_rate / bond.frequency;
  const double period = 1.0 / bond.frequency;
  for (int k = 0;; ++k) {
    const double t = bond.maturity - k * period;
    if (t <= kEpsilon) break;
    if (coupon != 0.0 || k == 0) flows.push_back({t, coupon});
  }
  std::reverse(flows.begin(), flows.end());
  flows.back().amount += bond.face;
  return flows;
}

}  // namespace ofod::instruments
