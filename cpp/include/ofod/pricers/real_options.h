// Copyright 2026 Denis Zhereshchin
//
// Real options (Hull, Ch 36): the market price of risk from CAPM, the
// risk-neutral valuation of a business by Monte Carlo, and the options to
// abandon, expand, contract or defer valued on a binomial lattice for the
// project's value and on the commodity tree of Ch 35.

#ifndef OFOD_PRICERS_REAL_OPTIONS_H_
#define OFOD_PRICERS_REAL_OPTIONS_H_

#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "ofod/pricers/commodity_tree.h"

namespace ofod::pricers {

// lambda = rho (mu_m - r) / sigma_m, where rho is the correlation of the
// variable with the market (Ch 36.2).
[[nodiscard]] double MarketPriceOfRisk(double correlation, double market_return,
                                       double rate, double market_vol);

// The risk-neutral growth rate m - lambda s of a variable with real-world
// growth m and volatility s.
[[nodiscard]] double RiskNeutralGrowth(double growth, double lambda,
                                       double volatility);

// Present value of cash flows at times `times` whose real-world expected
// values are `expected` (Ch 36.3): each is reduced by e^{-lambda s t}, the
// change of measure for a lognormal variable, and discounted at r.
[[nodiscard]] std::optional<double> RiskNeutralPresentValue(
    std::span<const double> times, std::span<const double> expected,
    double lambda, double volatility, double rate);

// Options on a project worth V, exercised at most once (exercising any
// option ends the flexibility):
struct Abandon {  // Sell the project for `salvage`.
  double salvage;
};
struct Expand {  // Scale by `factor` > 1 at a cost: factor V - cost.
  double factor;
  double cost;
};
struct Contract {  // Scale by `factor` < 1 and save: factor V + saving.
  double factor;
  double saving;
};
using ProjectOption = std::variant<Abandon, Expand, Contract>;

[[nodiscard]] double ExerciseValue(const ProjectOption& option, double value);

struct ProjectLattice {
  double value = 0.0;       // Today's value of the project's cash flows.
  double volatility = 0.0;  // Of the project value.
  double rate = 0.0;
  double payout = 0.0;  // Cash-flow yield, which acts like a dividend.
  double maturity = 0.0;
  int steps = 0;
};

// American-style project options exercisable at any step up to maturity on
// a Cox-Ross-Rubinstein lattice. Returns the value of the project plus its
// options, or nullopt for bad inputs or invalid probabilities.
[[nodiscard]] std::optional<double> ProjectWithOptions(
    const ProjectLattice& lattice, std::span<const ProjectOption> options);

// The option to defer an investment of cost `cost` up to maturity: an
// American call on the project value. Returns nullopt for bad inputs.
[[nodiscard]] std::optional<double> DeferralOption(
    const ProjectLattice& lattice, double cost);

// A resource project on the commodity tree (Hull's Ch 36.5 example):
// producing `production` units a period, each period's end cash flow is
// production (S - variable_cost) - fixed_cost while the project runs.
struct ExtractionProject {
  double production = 0.0;
  double variable_cost = 0.0;
  double fixed_cost = 0.0;
  double rate = 0.0;           // Discount rate (continuous).
  double abandon_value = 0.0;  // Received on abandoning at a node.
  double expand_factor = 1.0;  // Option to scale up at any node...
  double expand_cost = 0.0;    // ...for this cost.
};

struct ProjectValuation {
  double base;          // No options.
  double with_abandon;  // Option to abandon at any node.
  double with_expand;   // Option to expand at any node.
};

[[nodiscard]] std::optional<ProjectValuation> ValueExtractionProject(
    const CommodityTree& tree, const ExtractionProject& project);

// A business valued by Monte Carlo (Ch 36.4): revenue follows GBM with
// the risk-neutral growth rate, costs are fixed plus a fraction of revenue,
// cash accumulates at the risk-free rate, and the business is worth
// nothing once cash goes negative (it is bankrupt). At the horizon the
// business is valued at `terminal_multiple` times the final year's cash
// flow (if positive) plus cash.
struct BusinessSpec {
  double revenue = 0.0;  // Annual rate today.
  double growth = 0.0;   // Risk-neutral.
  double volatility = 0.0;
  double variable_cost = 0.0;  // Fraction of revenue.
  double fixed_cost = 0.0;     // Annual.
  double cash = 0.0;
  double rate = 0.0;
  double years = 0.0;
  int steps_per_year = 4;
  double terminal_multiple = 0.0;
};

struct BusinessValue {
  double value;
  double std_error;
  double bankruptcy_probability;
};

[[nodiscard]] std::optional<BusinessValue> ValueBusiness(
    const BusinessSpec& spec, int paths, std::uint64_t seed);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_REAL_OPTIONS_H_
