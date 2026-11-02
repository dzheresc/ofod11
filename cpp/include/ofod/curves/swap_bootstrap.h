// Copyright 2026 Denis Zhereshchin
//
// Zero curve from par swap rates, e.g. OIS/SOFR swaps (Hull, Ch 7.2).

#ifndef OFOD_CURVES_SWAP_BOOTSTRAP_H_
#define OFOD_CURVES_SWAP_BOOTSTRAP_H_

#include <optional>
#include <span>

#include "ofod/curves/zero_curve.h"

namespace ofod::curves {

struct SwapQuote {
  double maturity = 0.0;  // Years; payments every 1/frequency years.
  double par_rate = 0.0;
  int frequency = 1;
};

// A par swap's fixed leg is a bond priced at par, so each quote is
// bootstrapped as a bond with coupon = par rate and price 100.
[[nodiscard]] std::optional<ZeroCurve> BootstrapFromSwaps(
    std::span<const SwapQuote> quotes);

}  // namespace ofod::curves

#endif  // OFOD_CURVES_SWAP_BOOTSTRAP_H_
