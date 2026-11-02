// Copyright 2026 Denis Zhereshchin
//
// Chapter 21: Hull's American put (S0 = K = 50, r = 10%, sigma = 40%,
// T = 5 months) on binomial, trinomial and finite difference grids, and
// Monte Carlo variance reduction on a European call.

#include <array>
#include <format>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

#include "ofod/instruments/cash_flow.h"
#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/finite_difference.h"
#include "ofod/pricers/monte_carlo.h"

namespace {

namespace p = ofod::pricers;
using ofod::instruments::Exercise;
using ofod::instruments::OptionType;
using ofod::instruments::VanillaOption;

constexpr double kMaturity = 5.0 / 12.0;
constexpr p::TreeMarket kPutMarket{
    .spot = 50.0, .rate = 0.10, .yield = 0.0, .volatility = 0.40};

void PrintTrees(const VanillaOption& american) {
  std::cout << "American put, S0 = K = 50, r = 10%, sigma = 40%, T = 5/12\n";
  for (int steps : {5, 30, 50, 100, 500}) {
    const auto crr = p::PriceBinomial(american, kPutMarket, steps,
                                      p::TreeScheme::kCoxRossRubinstein);
    const auto tri = p::PriceTrinomial(american, kPutMarket, steps);
    const auto cv = p::ControlVariateAmericanPrice(american, kPutMarket, steps);
    if (!crr || !tri || !cv) continue;
    std::cout << std::format(
        "  {:>3} steps: binomial {:.4f}  trinomial {:.4f}  control variate "
        "{:.4f}\n",
        steps, crr->price, tri->price, *cv);
  }
  const VanillaOption european{OptionType::kPut, Exercise::kEuropean, 50.0,
                               kMaturity};
  if (const auto tree = p::PriceBinomial(european, kPutMarket, 5,
                                         p::TreeScheme::kCoxRossRubinstein)) {
    std::cout << std::format(
        "  European put: 5-step tree {:.2f}, Black-Scholes {:.2f}\n",
        tree->price,
        p::BsmPrice(OptionType::kPut, 50.0, 50.0, 0.10, 0.0, 0.40, kMaturity));
  }
  const std::array<ofod::instruments::CashFlow, 1> dividend = {
      {{3.5 / 12.0, 2.06}}};
  p::TreeMarket market = kPutMarket;
  market.spot = 52.0;
  if (const auto div =
          p::PriceBinomialWithDividends(american, market, 5, dividend)) {
    std::cout << std::format(
        "  S0 = 52, dividend 2.06 in 3.5 months: 5-step tree {:.2f}\n",
        div->price);
  }
}

void PrintFiniteDifferences(const VanillaOption& american) {
  const p::FdGrid hull{.s_min = 0.0,
                       .s_max = 100.0,
                       .price_steps = 20,
                       .time_steps = 10,
                       .log_space = false};
  const p::FdGrid fine{.s_min = 1.0,
                       .s_max = 500.0,
                       .price_steps = 400,
                       .time_steps = 400,
                       .log_space = true};
  const std::array<std::pair<std::string_view, std::optional<double>>, 5> rows =
      {{
          {"implicit, 20x10, Hull's projection",
           p::PriceFiniteDifference(american, kPutMarket, hull,
                                    p::FdScheme::kImplicit,
                                    p::AmericanSolver::kProjection)},
          {"implicit, 20x10, PSOR",
           p::PriceFiniteDifference(american, kPutMarket, hull,
                                    p::FdScheme::kImplicit)},
          {"explicit, 20x10",
           p::PriceFiniteDifference(american, kPutMarket, hull,
                                    p::FdScheme::kExplicit)},
          {"Crank-Nicolson, 20x10",
           p::PriceFiniteDifference(american, kPutMarket, hull,
                                    p::FdScheme::kCrankNicolson)},
          {"Crank-Nicolson, log grid 400x400",
           p::PriceFiniteDifference(american, kPutMarket, fine,
                                    p::FdScheme::kCrankNicolson)},
      }};
  std::cout << "Finite differences (Tables 21.5, 21.6)\n";
  for (const auto& [name, value] : rows) {
    if (value) std::cout << std::format("  {:<36} {:.4f}\n", name, *value);
  }
}

void PrintMonteCarlo() {
  const VanillaOption call{OptionType::kCall, Exercise::kEuropean, 40.0, 0.5};
  const p::TreeMarket market{
      .spot = 42.0, .rate = 0.10, .yield = 0.0, .volatility = 0.20};
  std::cout << std::format(
      "Monte Carlo, European call S0 = 42, K = 40, 10000 paths (BSM {:.4f})\n",
      p::BsmPrice(OptionType::kCall, 42.0, 40.0, 0.10, 0.0, 0.20, 0.5));
  constexpr std::array<std::pair<std::string_view, p::VarianceReduction>, 7>
      kMethods = {{
          {"plain", p::VarianceReduction::kNone},
          {"antithetic", p::VarianceReduction::kAntithetic},
          {"control variate", p::VarianceReduction::kControlVariate},
          {"importance sampling", p::VarianceReduction::kImportanceSampling},
          {"stratified", p::VarianceReduction::kStratified},
          {"moment matching", p::VarianceReduction::kMomentMatching},
          {"quasi-random (Sobol)", p::VarianceReduction::kQuasiRandom},
      }};
  for (const auto& [name, method] : kMethods) {
    const p::McOptions options{.paths = 10'000, .seed = 7, .method = method};
    if (const auto r = p::MonteCarloEuropean(call, market, options)) {
      std::cout << std::format("  {:<22} {:.4f}  s.e. {:.4f}\n", name, r->price,
                               r->std_error);
    }
  }
}

}  // namespace

int main() {
  const VanillaOption american{OptionType::kPut, Exercise::kAmerican, 50.0,
                               kMaturity};
  PrintTrees(american);
  PrintFiniteDifferences(american);
  PrintMonteCarlo();
  return 0;
}
