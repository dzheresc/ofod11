// Copyright 2026 Denis Zhereshchin
//
// Forward rate agreement (Hull, Ch 4.9).

#ifndef OFOD_INSTRUMENTS_FRA_H_
#define OFOD_INSTRUMENTS_FRA_H_

namespace ofod::instruments {

// Exchanges a fixed rate for the floating rate observed at `start`, on
// `notional`, over [start, end]. Both rates use simple compounding over the
// accrual period.
struct Fra {
  double notional = 0.0;
  double fixed_rate = 0.0;
  double start = 0.0;  // T1, years.
  double end = 0.0;    // T2, years.
};

}  // namespace ofod::instruments

#endif  // OFOD_INSTRUMENTS_FRA_H_
