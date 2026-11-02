// Copyright 2026 Denis Zhereshchin

#ifndef OFOD_INSTRUMENTS_CASH_FLOW_H_
#define OFOD_INSTRUMENTS_CASH_FLOW_H_

namespace ofod::instruments {

// An amount paid at a time measured in years from today.
struct CashFlow {
  double time;
  double amount;
};

}  // namespace ofod::instruments

#endif  // OFOD_INSTRUMENTS_CASH_FLOW_H_
