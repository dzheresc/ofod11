// Copyright 2026 Denis Zhereshchin
//
// No-arbitrage bounds and put-call parity for stock options (Hull, Ch 11).
// Rates are continuously compounded; `dividends_pv` is the present value of
// dividends paid during the option's life.

#ifndef OFOD_PRICERS_BOUNDS_H_
#define OFOD_PRICERS_BOUNDS_H_

namespace ofod::pricers {

// max(S - D - K e^{-rT}, 0) (eqs. 11.4, 11.8).
[[nodiscard]] double EuropeanCallLowerBound(double spot, double strike,
                                            double rate, double t,
                                            double dividends_pv = 0.0);
// max(D + K e^{-rT} - S, 0) (eqs. 11.5, 11.9).
[[nodiscard]] double EuropeanPutLowerBound(double spot, double strike,
                                           double rate, double t,
                                           double dividends_pv = 0.0);
// A call (European or American) is worth at most the stock (eq. 11.1).
[[nodiscard]] double CallUpperBound(double spot);
// K e^{-rT} (eq. 11.3).
[[nodiscard]] double EuropeanPutUpperBound(double strike, double rate,
                                           double t);
// K (eq. 11.2).
[[nodiscard]] double AmericanPutUpperBound(double strike);

// p = c + K e^{-rT} + D - S (eqs. 11.6, 11.10).
[[nodiscard]] double PutFromParity(double call, double spot, double strike,
                                   double rate, double t,
                                   double dividends_pv = 0.0);
// c = p + S - D - K e^{-rT}.
[[nodiscard]] double CallFromParity(double put, double spot, double strike,
                                    double rate, double t,
                                    double dividends_pv = 0.0);

struct Range {
  double lower;
  double upper;
};

// S - D - K <= C - P <= S - K e^{-rT} for American options (eqs. 11.7,
// 11.11).
[[nodiscard]] Range AmericanCallMinusPut(double spot, double strike,
                                         double rate, double t,
                                         double dividends_pv = 0.0);

// True when exercising an American call just before a dividend can be
// optimal: D > K (1 - e^{-r tau}), tau being the time to the next
// dividend or expiry (Ch 11 and Technical Note 4).
[[nodiscard]] bool CallEarlyExerciseMayBeOptimal(double dividend, double strike,
                                                 double rate,
                                                 double time_to_next);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_BOUNDS_H_
