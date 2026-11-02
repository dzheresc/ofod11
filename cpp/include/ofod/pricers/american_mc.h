// Copyright 2026 Denis Zhereshchin
//
// Least-squares Monte Carlo for American options (Longstaff-Schwartz,
// Hull Ch 27.8) and a convertible bond on a tree with a default intensity
// (Ch 27.4).

#ifndef OFOD_PRICERS_AMERICAN_MC_H_
#define OFOD_PRICERS_AMERICAN_MC_H_

#include <cstdint>
#include <limits>
#include <optional>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/monte_carlo.h"

namespace ofod::pricers {

using instruments::OptionType;

struct LsmOptions {
  int exercise_dates = 50;  // Equally spaced, the last at maturity.
  int paths = 100'000;
  std::uint64_t seed = 1;
  int basis_degree = 2;  // Regress on 1, x, ..., x^degree with x = S/K.
};

// American option by Longstaff-Schwartz. Paths are stored date by date in
// one contiguous block (paths x dates doubles) and the regression uses the
// in-the-money paths at each date. Returns std::nullopt for invalid
// options (dates < 1, paths < 2, degree outside [1, 6]).
[[nodiscard]] std::optional<McResult> LongstaffSchwartz(
    OptionType type, const TreeMarket& m, double strike, double t,
    const LsmOptions& options);

struct ConvertibleTerms {
  double face = 100.0;
  double conversion_ratio = 1.0;  // Shares received per bond.
  double maturity = 1.0;
  double call_price = std::numeric_limits<double>::infinity();
  double recovery = 0.4;  // Fraction of face recovered on default.
  double hazard = 0.0;    // Default intensity lambda; the stock falls to 0.
};

// Zero-coupon convertible on Hull's tree with default (Ch 27.4):
// u = exp(sqrt((sigma^2 - lambda) dt)), and at every node the value is
// max(conversion, min(hold, call price)). Returns std::nullopt unless
// steps >= 1 and sigma^2 dt > lambda dt with valid probabilities.
[[nodiscard]] std::optional<double> ConvertibleBondPrice(
    const TreeMarket& m, const ConvertibleTerms& terms, int steps);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_AMERICAN_MC_H_
