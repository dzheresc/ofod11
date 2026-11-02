// Copyright 2026 Denis Zhereshchin
//
// Plain vanilla interest rate swaps and fixed-for-fixed currency swaps
// (Hull, Ch 7).

#ifndef OFOD_INSTRUMENTS_SWAP_H_
#define OFOD_INSTRUMENTS_SWAP_H_

#include <vector>

namespace ofod::instruments {

// Fixed against floating on `notional`, `frequency` payments a year at
// `payment_times` (years from today, increasing). The floating rate for the
// next payment, set at the last reset, is `next_floating_rate`.
struct InterestRateSwap {
  double notional = 0.0;
  double fixed_rate = 0.0;
  int frequency = 2;
  std::vector<double> payment_times;
  double next_floating_rate = 0.0;
};

// Exchanges interest (and principal at maturity) in two currencies. Rates
// are annual coupons paid `frequency` times a year at `payment_times`.
struct CurrencySwap {
  double domestic_notional = 0.0;
  double domestic_rate = 0.0;
  double foreign_notional = 0.0;
  double foreign_rate = 0.0;
  int frequency = 1;
  std::vector<double> payment_times;
};

}  // namespace ofod::instruments

#endif  // OFOD_INSTRUMENTS_SWAP_H_
