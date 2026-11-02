// Copyright 2026 Denis Zhereshchin
//
// Chapter 2: daily settlement of a long gold futures position (Table 2.1).

#include <format>
#include <iostream>
#include <vector>

#include "ofod/futures/margin.h"

int main() {
  const ofod::futures::MarginSpec spec{.initial_margin = 6000.0,
                                       .maintenance_margin = 4500.0,
                                       .contract_size = 100.0,
                                       .contracts = 2};
  const std::vector<double> prices = {
      1250.0, 1241.0, 1238.3, 1244.6, 1241.3, 1240.1, 1236.2, 1229.9, 1230.8,
      1225.4, 1228.1, 1211.0, 1211.0, 1214.3, 1216.1, 1223.0, 1226.9};
  std::cout << "Day   Price   Daily   Cumul.  Balance  Call\n";
  int day = 0;
  for (const auto& d : ofod::futures::SimulateMarginAccount(spec, prices)) {
    std::cout << std::format(
        "{:>3} {:>8.2f} {:>7.0f} {:>8.0f} {:>8.0f} {:>5.0f}\n", day++, d.price,
        d.daily_gain, d.cumulative_gain, d.balance, d.margin_call);
  }
  return 0;
}
