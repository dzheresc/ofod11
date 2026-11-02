// Copyright 2026 Denis Zhereshchin
//
// Recombining binomial trees for vanilla options (Hull, Ch 13 and 21.1).
//
// A tree scheme maps (volatility, drift r - q, dt) to the up and down
// factors and the up probability. Options on an index, a currency or a
// futures contract use the same tree with q = dividend yield, foreign rate
// or r respectively.

#ifndef OFOD_PRICERS_BINOMIAL_TREE_H_
#define OFOD_PRICERS_BINOMIAL_TREE_H_

#include <concepts>
#include <optional>
#include <span>

#include "ofod/instruments/cash_flow.h"
#include "ofod/instruments/option.h"

namespace ofod::pricers {

struct TreeStep {
  double up;           // u: S -> S u.
  double down;         // d: S -> S d.
  double probability;  // Risk-neutral probability of an up move.
};

// p = (a - d) / (u - d) with a = growth over one step, e.g. e^{(r-q) dt}
// (eq. 13.3); for futures a = 1.
[[nodiscard]] double RiskNeutralProbability(double up, double down,
                                            double growth);

// Cox-Ross-Rubinstein: u = e^{sigma sqrt(dt)}, d = 1/u (eq. 13.13).
struct CoxRossRubinstein {
  static TreeStep Step(double volatility, double drift, double dt);
};

// Jarrow-Rudd (equal probabilities): u, d = e^{(drift - sigma^2/2) dt
// +/- sigma sqrt(dt)}, p = 1/2.
struct JarrowRudd {
  static TreeStep Step(double volatility, double drift, double dt);
};

// Tian: matches the first three moments of the lognormal distribution.
struct Tian {
  static TreeStep Step(double volatility, double drift, double dt);
};

template <class S>
concept TreeSchemePolicy = requires(double x) {
  { S::Step(x, x, x) } -> std::same_as<TreeStep>;
};

enum class TreeScheme { kCoxRossRubinstein, kJarrowRudd, kTian };

// Runtime counterpart of the policies above.
[[nodiscard]] TreeStep SchemeStep(TreeScheme scheme, double volatility,
                                  double drift, double dt);

struct TreeMarket {
  double spot = 0.0;
  double rate = 0.0;   // Continuously compounded risk-free rate.
  double yield = 0.0;  // Dividend yield, foreign rate, or r for futures.
  double volatility = 0.0;
};

struct TreeResult {
  double price;
  double delta;  // (f_u - f_d) / (S u - S d) at the first step (eq. 13.8).
};

// Backward induction on a tree with `steps` steps of length T / steps and
// the given step parameters, discounting at `rate`. American options are
// compared against intrinsic value at every node. Returns std::nullopt if
// steps < 1, u <= d or p lies outside [0, 1] (the tree admits arbitrage).
[[nodiscard]] std::optional<TreeResult> PriceTree(
    const instruments::VanillaOption& option, double spot, double rate,
    int steps, const TreeStep& step);

// Prices `option` with scheme S, e.g. PriceBinomial<CoxRossRubinstein>.
template <TreeSchemePolicy S>
[[nodiscard]] std::optional<TreeResult> PriceBinomial(
    const instruments::VanillaOption& option, const TreeMarket& market,
    int steps) {
  if (steps < 1) return std::nullopt;
  const double dt = option.maturity / steps;
  return PriceTree(option, market.spot, market.rate, steps,
                   S::Step(market.volatility, market.rate - market.yield, dt));
}

[[nodiscard]] std::optional<TreeResult> PriceBinomial(
    const instruments::VanillaOption& option, const TreeMarket& market,
    int steps, TreeScheme scheme);

// CRR tree for a stock paying known cash dividends (Ch 21.3): the tree is
// built for S* = S - PV(dividends) with volatility sigma, and the PV of the
// dividends still to be paid is added back at each node. `market.yield`
// must be zero.
[[nodiscard]] std::optional<TreeResult> PriceBinomialWithDividends(
    const instruments::VanillaOption& option, const TreeMarket& market,
    int steps, std::span<const instruments::CashFlow> dividends);

// Control variate on a CRR tree (Ch 21.3): American tree price plus the
// BSM European price minus the European tree price.
[[nodiscard]] std::optional<double> ControlVariateAmericanPrice(
    const instruments::VanillaOption& option, const TreeMarket& market,
    int steps);

// Trinomial tree (Ch 21.4): u = e^{sigma sqrt(3 dt)}, d = 1/u, p_m = 2/3,
// p_u and p_d = 1/6 +/- (r - q - sigma^2/2) sqrt(dt / (12 sigma^2)).
// Returns std::nullopt if steps < 1 or a probability is negative.
[[nodiscard]] std::optional<TreeResult> PriceTrinomial(
    const instruments::VanillaOption& option, const TreeMarket& market,
    int steps);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_BINOMIAL_TREE_H_
