// Copyright 2026 Denis Zhereshchin
//
// Chapter 3: minimum variance hedge ratio, number of contracts and
// changing the beta of an equity portfolio.

#include "ofod/risk/hedging.h"

#include <array>
#include <format>
#include <iostream>

int main() {
  namespace risk = ofod::risk;
  const double h = risk::MinimumVarianceHedgeRatio(0.928, 0.0263, 0.0313);
  std::cout << std::format(
      "Jet fuel: h* = {:.4f}, effectiveness {:.4f}, contracts {:.2f}\n", h,
      risk::HedgeEffectiveness(0.928),
      risk::OptimalContracts(0.78, 2'000'000.0, 42'000.0));

  const double portfolio = 5'050'000.0;
  const double futures = 1010.0 * 250.0;
  std::cout << std::format("Beta 1.5 portfolio: hedge with {:.0f} contracts\n",
                           risk::BetaHedgeContracts(1.5, portfolio, futures));
  for (double target : {0.75, 2.0}) {
    std::cout << std::format(
        "  to beta {:.2f}: {:+.0f} contracts\n", target,
        risk::ChangeBetaContracts(1.5, target, portfolio, futures));
  }
  const std::array<risk::RollLeg, 3> legs = {
      {{68.20, 67.40}, {67.00, 66.50}, {66.30, 65.90}}};
  std::cout << std::format("Stack and roll gain per barrel: {:.2f}\n",
                           risk::ShortRollGain(legs));
  return 0;
}
