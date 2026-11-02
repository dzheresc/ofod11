// Copyright 2026 Denis Zhereshchin
//
// Daily settlement of a futures margin account (Hull, Ch 2.4).

#ifndef OFOD_FUTURES_MARGIN_H_
#define OFOD_FUTURES_MARGIN_H_

#include <span>
#include <vector>

#include "ofod/instruments/payoff.h"

namespace ofod::futures {

struct MarginSpec {
  double initial_margin = 0.0;      // Per contract.
  double maintenance_margin = 0.0;  // Per contract.
  double contract_size = 1.0;       // Units of the asset per contract.
  int contracts = 1;
  instruments::Side side = instruments::Side::kLong;
};

// One row of Hull's Table 2.1.
struct MarginDay {
  double price;
  double daily_gain;
  double cumulative_gain;
  double balance;      // Before any margin call is paid.
  double margin_call;  // Paid to restore the initial margin; 0 if none.
};

// Simulates the account. `prices[0]` is the trade price; each later price is
// a daily settlement price. A margin call brings the balance back up to the
// initial margin whenever it falls below the maintenance margin. Returns one
// row per price, the first being the opening deposit.
[[nodiscard]] std::vector<MarginDay> SimulateMarginAccount(
    const MarginSpec& spec, std::span<const double> prices);

}  // namespace ofod::futures

#endif  // OFOD_FUTURES_MARGIN_H_
