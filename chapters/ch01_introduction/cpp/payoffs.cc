// Copyright 2026 Denis Zhereshchin
//
// Chapter 1: payoffs and profits of the six basic positions.

#include <format>
#include <iostream>

#include "ofod/instruments/payoff.h"

int main() {
  using ofod::instruments::CallPayoff;
  using ofod::instruments::ForwardPayoff;
  using ofod::instruments::Position;
  using ofod::instruments::PutPayoff;
  using ofod::instruments::Side;

  constexpr double kStrike = 100.0;
  constexpr double kPremium = 5.0;
  const Position<ForwardPayoff> long_forward{.payoff = {kStrike}};
  const Position<ForwardPayoff> short_forward{.payoff = {kStrike},
                                              .side = Side::kShort};
  const Position<CallPayoff> long_call{.payoff = {kStrike},
                                       .premium = kPremium};
  const Position<CallPayoff> short_call{
      .payoff = {kStrike}, .side = Side::kShort, .premium = kPremium};
  const Position<PutPayoff> long_put{.payoff = {kStrike}, .premium = kPremium};
  const Position<PutPayoff> short_put{
      .payoff = {kStrike}, .side = Side::kShort, .premium = kPremium};

  std::cout << "Profit at expiry, K = 100, option premium = 5\n";
  std::cout << std::format("{:>6} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8}\n", "S_T",
                           "long F", "short F", "long C", "short C", "long P",
                           "short P");
  for (double s = 70.0; s <= 130.0; s += 10.0) {
    std::cout << std::format(
        "{:6.0f} {:8.1f} {:8.1f} {:8.1f} {:8.1f} {:8.1f} {:8.1f}\n", s,
        long_forward.ProfitAt(s), short_forward.ProfitAt(s),
        long_call.ProfitAt(s), short_call.ProfitAt(s), long_put.ProfitAt(s),
        short_put.ProfitAt(s));
  }
  return 0;
}
