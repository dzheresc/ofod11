// Copyright 2026 Denis Zhereshchin
//
// Short-term interest rate futures and duration hedging (Hull, Ch 6.3-6.5).

#ifndef OFOD_FUTURES_RATE_FUTURES_H_
#define OFOD_FUTURES_RATE_FUTURES_H_

namespace ofod::futures {

// Futures rate implied by a quote Q: (100 - Q) / 100.
[[nodiscard]] double FuturesRateFromQuote(double quote);

// Value of one three-month Eurodollar/SOFR contract:
// 10,000 * (100 - 0.25 * (100 - Q)), i.e. $25 per basis point.
[[nodiscard]] double ThreeMonthContractValue(double quote);

// Ho-Lee convexity adjustment (eq. 6.3): forward rate = futures rate -
// sigma^2 t1 t2 / 2, with t1 the futures maturity and t2 the end of the
// underlying rate period. Rates are continuously compounded.
[[nodiscard]] double ConvexityAdjustment(double sigma, double t1, double t2);

// Duration-based hedge N* = P D_P / (V_F D_F) (eq. 6.5).
[[nodiscard]] double DurationHedgeContracts(double portfolio_value,
                                            double portfolio_duration,
                                            double futures_value,
                                            double futures_duration);

}  // namespace ofod::futures

#endif  // OFOD_FUTURES_RATE_FUTURES_H_
