// Copyright 2026 Denis Zhereshchin
//
// Option trading strategies as portfolios (Hull, Ch 12). Arguments come in
// (strike, premium) pairs with strikes in increasing order.

#ifndef OFOD_STRATEGIES_STRATEGIES_H_
#define OFOD_STRATEGIES_STRATEGIES_H_

#include <optional>
#include <span>

#include "ofod/instruments/portfolio.h"

namespace ofod::strategies {

using instruments::Portfolio;

// Long stock (bought at `spot`) plus a short call.
[[nodiscard]] Portfolio CoveredCall(double spot, double strike, double call);
// Long stock plus a long put.
[[nodiscard]] Portfolio ProtectivePut(double spot, double strike, double put);

// Long call K1, short call K2.
[[nodiscard]] Portfolio BullCallSpread(double k1, double c1, double k2,
                                       double c2);
// Long put K1, short put K2.
[[nodiscard]] Portfolio BullPutSpread(double k1, double p1, double k2,
                                      double p2);
// Short call K1, long call K2.
[[nodiscard]] Portfolio BearCallSpread(double k1, double c1, double k2,
                                       double c2);
// Short put K1, long put K2.
[[nodiscard]] Portfolio BearPutSpread(double k1, double p1, double k2,
                                      double p2);
// Bull call spread plus bear put spread; pays K2 - K1 for sure.
[[nodiscard]] Portfolio BoxSpread(double k1, double c1, double p1, double k2,
                                  double c2, double p2);
// Long calls K1 and K3, two short calls K2.
[[nodiscard]] Portfolio ButterflyCallSpread(double k1, double c1, double k2,
                                            double c2, double k3, double c3);

// Long call and long put at the same strike.
[[nodiscard]] Portfolio Straddle(double strike, double call, double put);
// One call and two puts.
[[nodiscard]] Portfolio Strip(double strike, double call, double put);
// Two calls and one put.
[[nodiscard]] Portfolio Strap(double strike, double call, double put);
// Long put K1 and long call K2 > K1.
[[nodiscard]] Portfolio Strangle(double put_strike, double put,
                                 double call_strike, double call);

// Amount left to buy options after funding the zero-coupon bond that
// protects the principal: P (1 - e^{-rT}) (Ch 12.2).
[[nodiscard]] double PrincipalProtectedBudget(double principal, double rate,
                                              double t);

// Replicates the piecewise-linear payoff
//   f(S) = value_at_zero + slopes[0] S + sum_i (slopes[i+1] - slopes[i])
//          max(S - kinks[i], 0)
// with cash, the stock and calls (Ch 12.5). Returns std::nullopt unless
// slopes.size() == kinks.size() + 1 and the kinks are strictly increasing.
[[nodiscard]] std::optional<Portfolio> ReplicatePayoff(
    double value_at_zero, std::span<const double> slopes,
    std::span<const double> kinks);

}  // namespace ofod::strategies

#endif  // OFOD_STRATEGIES_STRATEGIES_H_
