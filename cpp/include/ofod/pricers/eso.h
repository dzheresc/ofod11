// Copyright 2026 Denis Zhereshchin
//
// Employee stock options (Hull, Ch 16): the Hull-White binomial model with
// a vesting period, an early-exercise multiple and an employee exit rate,
// and the expected-life approximation.

#ifndef OFOD_PRICERS_ESO_H_
#define OFOD_PRICERS_ESO_H_

#include <optional>

#include "ofod/pricers/binomial_tree.h"

namespace ofod::pricers {

struct EsoTerms {
  double strike = 0.0;
  double maturity = 0.0;
  double vesting = 0.0;            // Years before the option can be exercised.
  double exercise_multiple = 0.0;  // Exercise once S >= multiple * K.
  double exit_rate = 0.0;          // Employees leaving per year.
};

// CRR tree on `market`. At each node after vesting the option is exercised
// if S >= M K; otherwise, with probability 1 - e^{-exit_rate dt}, the
// employee leaves and exercises if in the money. Before vesting a leaver
// forfeits the option. Returns std::nullopt for steps < 1, M < 1, a
// negative exit rate or vesting outside [0, T].
[[nodiscard]] std::optional<double> PriceEmployeeStockOption(
    const EsoTerms& terms, const TreeMarket& market, int steps);

// BSM call with the expected life in place of the contractual maturity
// (Ch 16.4, the approach once allowed by FAS 123).
[[nodiscard]] double ExpectedLifeValue(double spot, double strike, double rate,
                                       double yield, double volatility,
                                       double expected_life);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_ESO_H_
