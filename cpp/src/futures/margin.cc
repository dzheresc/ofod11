// Copyright 2026 Denis Zhereshchin

#include "ofod/futures/margin.h"

#include <cstddef>

namespace ofod::futures {

std::vector<MarginDay> SimulateMarginAccount(const MarginSpec& spec,
                                             std::span<const double> prices) {
  std::vector<MarginDay> days;
  if (prices.empty()) return days;
  const double initial = spec.initial_margin * spec.contracts;
  const double maintenance = spec.maintenance_margin * spec.contracts;
  const double units =
      instruments::Sign(spec.side) * spec.contract_size * spec.contracts;
  days.push_back({prices[0], 0.0, 0.0, initial, 0.0});
  double balance = initial;
  double cumulative = 0.0;
  for (std::size_t i = 1; i < prices.size(); ++i) {
    const double gain = units * (prices[i] - prices[i - 1]);
    cumulative += gain;
    balance += gain;
    const double call = balance < maintenance ? initial - balance : 0.0;
    days.push_back({prices[i], gain, cumulative, balance, call});
    balance += call;
  }
  return days;
}

}  // namespace ofod::futures
