// Copyright 2026 Denis Zhereshchin
//
// Chapter 8: losses on ABS tranches (equity 0-5%, mezzanine 5-20%,
// senior 20-100%) for a range of pool losses.

#include <format>
#include <iostream>
#include <vector>

#include "ofod/credit/tranche.h"

int main() {
  const std::vector<ofod::credit::Tranche> tranches = {
      {0.0, 0.05}, {0.05, 0.20}, {0.20, 1.0}};
  std::cout << "Pool loss  Equity  Mezz.  Senior\n";
  for (double pool : {0.02, 0.05, 0.10, 0.15, 0.20, 0.25, 0.35}) {
    const auto l = ofod::credit::WaterfallLosses(tranches, pool);
    std::cout << std::format("{:>8.0f}% {:>6.1f}% {:>5.1f}% {:>6.1f}%\n",
                             100.0 * pool, 100.0 * l[0], 100.0 * l[1],
                             100.0 * l[2]);
  }
  return 0;
}
