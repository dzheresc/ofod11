// Copyright 2026 Denis Zhereshchin

#include "ofod/strategies/strategies.h"

#include <cmath>
#include <cstddef>

#include "ofod/instruments/payoff.h"

namespace ofod::strategies {

namespace {

using instruments::CallPayoff;
using instruments::CashPayoff;
using instruments::PutPayoff;
using instruments::StockPayoff;

}  // namespace

Portfolio CoveredCall(double spot, double strike, double call) {
  Portfolio p;
  p.Add(1.0, StockPayoff{}, spot).Add(-1.0, CallPayoff{strike}, call);
  return p;
}

Portfolio ProtectivePut(double spot, double strike, double put) {
  Portfolio p;
  p.Add(1.0, StockPayoff{}, spot).Add(1.0, PutPayoff{strike}, put);
  return p;
}

Portfolio BullCallSpread(double k1, double c1, double k2, double c2) {
  Portfolio p;
  p.Add(1.0, CallPayoff{k1}, c1).Add(-1.0, CallPayoff{k2}, c2);
  return p;
}

Portfolio BullPutSpread(double k1, double p1, double k2, double p2) {
  Portfolio p;
  p.Add(1.0, PutPayoff{k1}, p1).Add(-1.0, PutPayoff{k2}, p2);
  return p;
}

Portfolio BearCallSpread(double k1, double c1, double k2, double c2) {
  Portfolio p;
  p.Add(-1.0, CallPayoff{k1}, c1).Add(1.0, CallPayoff{k2}, c2);
  return p;
}

Portfolio BearPutSpread(double k1, double p1, double k2, double p2) {
  Portfolio p;
  p.Add(-1.0, PutPayoff{k1}, p1).Add(1.0, PutPayoff{k2}, p2);
  return p;
}

Portfolio BoxSpread(double k1, double c1, double p1, double k2, double c2,
                    double p2) {
  Portfolio p;
  p.Add(1.0, CallPayoff{k1}, c1)
      .Add(-1.0, CallPayoff{k2}, c2)
      .Add(-1.0, PutPayoff{k1}, p1)
      .Add(1.0, PutPayoff{k2}, p2);
  return p;
}

Portfolio ButterflyCallSpread(double k1, double c1, double k2, double c2,
                              double k3, double c3) {
  Portfolio p;
  p.Add(1.0, CallPayoff{k1}, c1)
      .Add(-2.0, CallPayoff{k2}, c2)
      .Add(1.0, CallPayoff{k3}, c3);
  return p;
}

Portfolio Straddle(double strike, double call, double put) {
  Portfolio p;
  p.Add(1.0, CallPayoff{strike}, call).Add(1.0, PutPayoff{strike}, put);
  return p;
}

Portfolio Strip(double strike, double call, double put) {
  Portfolio p;
  p.Add(1.0, CallPayoff{strike}, call).Add(2.0, PutPayoff{strike}, put);
  return p;
}

Portfolio Strap(double strike, double call, double put) {
  Portfolio p;
  p.Add(2.0, CallPayoff{strike}, call).Add(1.0, PutPayoff{strike}, put);
  return p;
}

Portfolio Strangle(double put_strike, double put, double call_strike,
                   double call) {
  Portfolio p;
  p.Add(1.0, PutPayoff{put_strike}, put)
      .Add(1.0, CallPayoff{call_strike}, call);
  return p;
}

double PrincipalProtectedBudget(double principal, double rate, double t) {
  return principal * (1.0 - std::exp(-rate * t));
}

std::optional<Portfolio> ReplicatePayoff(double value_at_zero,
                                         std::span<const double> slopes,
                                         std::span<const double> kinks) {
  if (slopes.size() != kinks.size() + 1) return std::nullopt;
  for (std::size_t i = 1; i < kinks.size(); ++i) {
    if (kinks[i] <= kinks[i - 1]) return std::nullopt;
  }
  Portfolio p;
  if (value_at_zero != 0.0) p.Add(1.0, CashPayoff{value_at_zero});
  if (slopes[0] != 0.0) p.Add(slopes[0], StockPayoff{});
  for (std::size_t i = 0; i < kinks.size(); ++i) {
    const double change = slopes[i + 1] - slopes[i];
    if (change != 0.0) p.Add(change, CallPayoff{kinks[i]});
  }
  return p;
}

}  // namespace ofod::strategies
