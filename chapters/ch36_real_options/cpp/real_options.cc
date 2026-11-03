// Copyright 2026 Denis Zhereshchin
//
// Chapter 36: the market price of risk, risk-neutral valuation of cash
// flows and of a business, options to abandon, expand, contract and defer
// on a project lattice, and a resource project on the Ch 35 tree.

#include "ofod/pricers/real_options.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ofod/pricers/commodity_tree.h"

namespace {

namespace p = ofod::pricers;

// Unwraps a result the example inputs guarantee; main reports a failure.
template <typename T>
T Must(std::optional<T> value) {
  if (!value) throw std::runtime_error("unexpected pricing failure");
  return *std::move(value);
}

void RiskNeutral() {
  const double lambda = p::MarketPriceOfRisk(0.7, 0.10, 0.04, 0.20);
  std::cout << std::format(
      "Market price of risk, rho = 0.7, market excess 6%, market vol 20%: "
      "{:.4f}\n  sales growing at 8% with 30% vol grow at {:.4f} in the "
      "risk-neutral world\n",
      lambda, p::RiskNeutralGrowth(0.08, lambda, 0.3));
  const std::vector<double> times = {1.0, 2.0, 3.0};
  const std::vector<double> expected = {100.0, 110.0, 120.0};
  double capm = 0.0;
  const double beta_rate = 0.04 + 0.7 * 0.3 / 0.2 * 0.06;  // r + beta (mu - r).
  for (std::size_t i = 0; i < times.size(); ++i) {
    capm += expected[i] * std::exp(-beta_rate * times[i]);
  }
  std::cout << std::format(
      "  PV of expected flows 100, 110, 120: risk-neutral {:.4f}, at the "
      "CAPM rate {:.4f}%: {:.4f}\n",
      Must(p::RiskNeutralPresentValue(times, expected, lambda, 0.3, 0.04)),
      100.0 * beta_rate, capm);
}

void Business() {
  p::BusinessSpec spec{.revenue = 50.0,
                       .growth = 0.03,
                       .volatility = 0.0,
                       .variable_cost = 0.6,
                       .fixed_cost = 18.0,
                       .cash = 5.0,
                       .rate = 0.04,
                       .years = 5.0,
                       .steps_per_year = 4,
                       .terminal_multiple = 8.0};
  std::cout << "Business: revenue 50 growing at 3% (risk-neutral), costs 60% "
               "+ 18, cash 5, 8x terminal multiple\n";
  for (double vol : {0.0, 0.2, 0.4, 0.6}) {
    spec.volatility = vol;
    const auto v = Must(p::ValueBusiness(spec, 20'000, 7));
    std::cout << std::format(
        "  revenue vol {:3.0f}%: value {:8.4f} (se {:.4f}), P(bankrupt) "
        "{:.4f}\n",
        100.0 * vol, v.value, v.std_error, v.bankruptcy_probability);
  }
}

void Lattice() {
  const p::ProjectLattice lattice{.value = 100.0,
                                  .volatility = 0.3,
                                  .rate = 0.05,
                                  .payout = 0.04,
                                  .maturity = 2.0,
                                  .steps = 200};
  const std::array<p::ProjectOption, 1> abandon = {p::Abandon{90.0}};
  const std::array<p::ProjectOption, 1> expand = {p::Expand{1.3, 25.0}};
  const std::array<p::ProjectOption, 1> contract = {p::Contract{0.7, 25.0}};
  const std::array<p::ProjectOption, 3> all = {
      p::Abandon{90.0}, p::Expand{1.3, 25.0}, p::Contract{0.7, 25.0}};
  std::cout << std::format(
      "Project worth 100, 30% vol, 4% payout, options for 2 years (200 "
      "steps):\n  abandon for 90: {:.4f}; expand 30% for 25: {:.4f}; "
      "contract 30% saving 25: {:.4f}; all three: {:.4f}\n",
      Must(p::ProjectWithOptions(lattice, abandon)),
      Must(p::ProjectWithOptions(lattice, expand)),
      Must(p::ProjectWithOptions(lattice, contract)),
      Must(p::ProjectWithOptions(lattice, all)));
  std::cout << std::format(
      "  invest 105 now: NPV {:.4f}; option to defer up to 2 years: "
      "{:.4f}\n",
      100.0 - 105.0, Must(p::DeferralOption(lattice, 105.0)));
  std::cout << "  value of the abandonment option by volatility:";
  for (double vol : {0.1, 0.2, 0.3, 0.4, 0.5}) {
    p::ProjectLattice l = lattice;
    l.volatility = vol;
    std::cout << std::format(" {:.0f}%: {:.4f}", 100.0 * vol,
                             Must(p::ProjectWithOptions(l, abandon)) - 100.0);
  }
  std::cout << '\n';
}

void Extraction() {
  const std::vector<double> futures = {22.0, 23.0, 24.0};
  const auto tree = p::CommodityTree::Build(20.0, futures, 0.1, 0.2, 1.0);
  if (!tree) throw std::runtime_error("tree construction failed");
  const auto v = Must(p::ValueExtractionProject(*tree, {.production = 2.0,
                                                        .variable_cost = 17.0,
                                                        .fixed_cost = 6.0,
                                                        .rate = 0.1,
                                                        .abandon_value = 0.0,
                                                        .expand_factor = 1.2,
                                                        .expand_cost = 2.0}));
  std::cout << std::format(
      "Resource project on the Ch 35 tree, 2 units a year for 3 years, "
      "variable cost 17, fixed 6:\n  base {:.4f}; with abandonment {:.4f} "
      "(option {:.4f}); with 20% expansion for 2 {:.4f} (option {:.4f})\n",
      v.base, v.with_abandon, v.with_abandon - v.base, v.with_expand,
      v.with_expand - v.base);
}

}  // namespace

int main() {
  try {
    RiskNeutral();
    Business();
    Lattice();
    Extraction();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
