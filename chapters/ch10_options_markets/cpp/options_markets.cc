// Copyright 2026 Denis Zhereshchin
//
// Chapter 10: option payoffs, contract adjustments and naked option margin.

#include <format>
#include <iostream>

#include "ofod/instruments/option.h"

int main() {
  namespace ins = ofod::instruments;
  const ins::VanillaOption call{.strike = 40.0};
  for (double s : {30.0, 40.0, 50.0}) {
    std::cout << std::format("Call K=40 at S_T={:.0f}: payoff {:.0f}\n", s,
                             ins::IntrinsicValue(call, s));
  }
  const auto split = ins::AdjustForSplit({30.0, 100.0}, 3, 2);
  std::cout << std::format("3-for-2 split: K 30 -> {:.2f}, {:.0f} shares\n",
                           split.strike, split.shares);
  const auto div = ins::AdjustForStockDividend({15.0, 100.0}, 0.25);
  std::cout << std::format(
      "25% stock dividend: K 15 -> {:.2f}, {:.0f} shares\n", div.strike,
      div.shares);
  const double margin =
      ins::NakedOptionMargin(ins::OptionType::kCall, 5.0, 38.0, 40.0);
  std::cout << std::format(
      "Naked calls: 4 contracts, c=5, K=40, S=38 -> margin {:.0f}\n",
      400.0 * margin);
  return 0;
}
