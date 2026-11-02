// Copyright 2026 Denis Zhereshchin
//
// Chapter 19: Greeks of Hull's 100,000-call position, bumped vs analytic
// Greeks, gamma and vega neutral hedges, a scenario grid, and the
// delta-hedging performance table.

#include "ofod/greeks/greeks.h"

#include <array>
#include <format>
#include <iostream>

#include "ofod/pricers/black_scholes.h"

namespace {

namespace g = ofod::greeks;
using ofod::instruments::OptionType;

constexpr double kStrike = 50.0;
constexpr double kMaturity = 20.0 / 52.0;

double Call(const g::OptionInputs& x) {
  return ofod::pricers::BsmPrice(OptionType::kCall, x.spot, kStrike, x.rate,
                                 x.yield, x.volatility, x.maturity);
}

}  // namespace

int main() {
  const g::OptionInputs in{.spot = 49.0,
                           .rate = 0.05,
                           .yield = 0.0,
                           .volatility = 0.20,
                           .maturity = kMaturity};
  const g::Greeks a = g::BsmGreeks(OptionType::kCall, kStrike, in);
  const g::Greeks b = g::BumpGreeks(Call, in);
  std::cout << std::format(
      "Call S0 = 49, K = 50, r = 5%, sigma = 20%, 20 weeks: price {:.4f}\n",
      Call(in));
  std::cout << "            analytic     bumped   Hull's units\n";
  std::cout << std::format("  delta  {:>11.6f} {:>10.6f}   {:.3f}\n", a.delta,
                           b.delta, a.delta);
  std::cout << std::format("  gamma  {:>11.6f} {:>10.6f}   {:.3f}\n", a.gamma,
                           b.gamma, a.gamma);
  std::cout << std::format("  vega   {:>11.6f} {:>10.6f}   {:.3f} per 1% vol\n",
                           a.vega, b.vega, a.vega / 100.0);
  std::cout << std::format(
      "  theta  {:>11.6f} {:>10.6f}   {:.4f} per calendar day\n", a.theta,
      b.theta, a.theta / 365.0);
  std::cout << std::format(
      "  rho    {:>11.6f} {:>10.6f}   {:.4f} per 1% rate\n", a.rho, b.rho,
      a.rho / 100.0);

  if (const auto h = g::GammaNeutralHedge({.delta = 0.0, .gamma = -3000.0},
                                          {.delta = 0.62, .gamma = 1.5})) {
    std::cout << std::format(
        "Gamma neutral: buy {:.0f} options, trade {:.0f} shares\n", h->option1,
        h->underlying);
  }
  if (const auto h = g::GammaVegaNeutralHedge(
          {.delta = 0.0, .gamma = -5000.0, .vega = -8000.0},
          {.delta = 0.6, .gamma = 0.5, .vega = 2.0},
          {.delta = 0.5, .gamma = 0.8, .vega = 1.2})) {
    std::cout << std::format(
        "Gamma and vega neutral: {:.0f} of option 1, {:.0f} of option 2, "
        "trade {:.0f} shares\n",
        h->option1, h->option2, h->underlying);
  }

  constexpr std::array<double, 5> kSpots = {45.0, 47.0, 49.0, 51.0, 53.0};
  constexpr std::array<double, 3> kVols = {0.15, 0.20, 0.25};
  const auto grid = g::ScenarioGrid(Call, in, kSpots, kVols);
  std::cout << "Scenario grid: change in value of one short call\n  vol\\S";
  for (double s : kSpots) std::cout << std::format("{:>9.0f}", s);
  std::cout << '\n';
  for (std::size_t i = 0; i < kVols.size(); ++i) {
    std::cout << std::format("  {:>4.0f}%", 100.0 * kVols[i]);
    for (double v : grid[i]) std::cout << std::format("{:>9.4f}", 0.0 - v);
    std::cout << '\n';
  }

  std::cout << "Delta hedging performance (std of cost / BSM price), "
               "mu = 13%, 20,000 paths\n";
  const g::HedgeExperiment e{.spot = 49.0,
                             .strike = kStrike,
                             .rate = 0.05,
                             .drift = 0.13,
                             .volatility = 0.20,
                             .maturity = kMaturity};
  for (int n : {4, 5, 10, 20, 40, 80}) {
    if (const auto r = g::SimulateDeltaHedge(e, n, 20000, 1)) {
      std::cout << std::format(
          "  rebalance every {:>4.2f} weeks: {:.3f} (mean cost {:.3f})\n",
          20.0 / n, r->ratio, r->mean_cost);
    }
  }
  return 0;
}
