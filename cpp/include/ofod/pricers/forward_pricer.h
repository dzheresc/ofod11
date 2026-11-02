// Copyright 2026 Denis Zhereshchin
//
// Forward prices and forward contract values (Hull, Ch 5).
// All rates are continuously compounded; times are in years.

#ifndef OFOD_PRICERS_FORWARD_PRICER_H_
#define OFOD_PRICERS_FORWARD_PRICER_H_

#include <span>

#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/cash_flow.h"
#include "ofod/instruments/payoff.h"

namespace ofod::pricers {

// F0 = S0 exp(rT) for an investment asset with no income (eq. 5.1).
[[nodiscard]] double ForwardPrice(double spot, double rate, double t);

// F0 = (S0 - I) exp(rT), I = present value of income (eq. 5.2).
[[nodiscard]] double ForwardPriceWithIncome(double spot, double income_pv,
                                            double rate, double t);

// F0 = S0 exp((r - q) T) for a known yield q (eq. 5.3). Also covers stock
// indices (q = dividend yield) and currencies (q = foreign rate, eq. 5.9).
[[nodiscard]] double ForwardPriceWithYield(double spot, double rate,
                                           double yield, double t);

// F0 = S0 exp((r + u - y) T): storage cost u and convenience yield y as
// proportions of the spot price (cost of carry, Ch 5.11-5.12).
[[nodiscard]] double ForwardPriceCostOfCarry(double spot, double rate,
                                             double storage_yield,
                                             double convenience_yield,
                                             double t);

// Present value of `flows` discounted on `curve`.
[[nodiscard]] double PresentValue(std::span<const instruments::CashFlow> flows,
                                  const curves::ZeroCurve& curve);

// Value of a forward with delivery price `delivery_price` when today's
// forward price is `forward_price`: (F0 - K) exp(-rT) for a long (eq. 5.4).
[[nodiscard]] double ForwardContractValue(double forward_price,
                                          double delivery_price, double rate,
                                          double t, instruments::Side side);

enum class ForwardArbitrage {
  kNone,
  kCashAndCarry,         // Forward too rich: buy spot, sell forward.
  kReverseCashAndCarry,  // Forward too cheap: short spot, buy forward.
};

// Classifies a quoted forward against its theoretical price. Differences
// within `tolerance` are treated as no arbitrage.
[[nodiscard]] ForwardArbitrage DetectForwardArbitrage(double quoted_forward,
                                                      double theoretical,
                                                      double tolerance = 1e-9);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_FORWARD_PRICER_H_
