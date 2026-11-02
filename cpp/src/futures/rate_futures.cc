// Copyright 2026 Denis Zhereshchin

#include "ofod/futures/rate_futures.h"

namespace ofod::futures {

double FuturesRateFromQuote(double quote) { return (100.0 - quote) / 100.0; }

double ThreeMonthContractValue(double quote) {
  return 10000.0 * (100.0 - 0.25 * (100.0 - quote));
}

double ConvexityAdjustment(double sigma, double t1, double t2) {
  return 0.5 * sigma * sigma * t1 * t2;
}

double DurationHedgeContracts(double portfolio_value, double portfolio_duration,
                              double futures_value, double futures_duration) {
  return portfolio_value * portfolio_duration /
         (futures_value * futures_duration);
}

}  // namespace ofod::futures
