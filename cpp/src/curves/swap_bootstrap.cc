// Copyright 2026 Denis Zhereshchin

#include "ofod/curves/swap_bootstrap.h"

#include <vector>

#include "ofod/curves/bond_bootstrap.h"

namespace ofod::curves {

std::optional<ZeroCurve> BootstrapFromSwaps(std::span<const SwapQuote> quotes) {
  std::vector<BondQuote> bonds;
  bonds.reserve(quotes.size());
  for (const auto& q : quotes) {
    bonds.push_back({instruments::Bond{.face = 100.0,
                                       .coupon_rate = q.par_rate,
                                       .maturity = q.maturity,
                                       .frequency = q.frequency},
                     100.0});
  }
  return BootstrapFromBonds(bonds);
}

}  // namespace ofod::curves
