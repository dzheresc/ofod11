// Copyright 2026 Denis Zhereshchin
//
// Chapter 13: one- and two-step trees, American exercise, and convergence
// of CRR, Jarrow-Rudd and Tian trees as the number of steps grows.

#include <cmath>
#include <format>
#include <iostream>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"

int main() {
  namespace p = ofod::pricers;
  using ofod::instruments::Exercise;
  using ofod::instruments::OptionType;
  using ofod::instruments::VanillaOption;

  const VanillaOption call{.strike = 21.0, .maturity = 0.25};
  const double prob = p::RiskNeutralProbability(1.1, 0.9, std::exp(0.03));
  if (const auto r = p::PriceTree(call, 20.0, 0.12, 1, {1.1, 0.9, prob})) {
    std::cout << std::format(
        "One step: p = {:.4f}, call {:.3f}, delta {:.2f}\n", prob, r->price,
        r->delta);
  }

  VanillaOption put{
      .option_type = OptionType::kPut, .strike = 52.0, .maturity = 2.0};
  const double q = p::RiskNeutralProbability(1.2, 0.8, std::exp(0.05));
  const auto european = p::PriceTree(put, 50.0, 0.05, 2, {1.2, 0.8, q});
  put.exercise = Exercise::kAmerican;
  const auto american = p::PriceTree(put, 50.0, 0.05, 2, {1.2, 0.8, q});
  if (!european || !american) return 1;
  std::cout << std::format("Two-step put: European {:.4f}, American {:.4f}\n",
                           european->price, american->price);

  const VanillaOption bsm_call{.strike = 40.0, .maturity = 0.5};
  const p::TreeMarket m{.spot = 42.0, .rate = 0.10, .volatility = 0.20};
  std::cout << "Steps      CRR       JR     Tian   (BSM 4.7594)\n";
  for (int n : {10, 11, 50, 51, 100, 500}) {
    std::cout << std::format("{:>5}", n);
    for (auto s : {p::TreeScheme::kCoxRossRubinstein,
                   p::TreeScheme::kJarrowRudd, p::TreeScheme::kTian}) {
      const auto r = p::PriceBinomial(bsm_call, m, n, s);
      std::cout << std::format(" {:>8.4f}", r ? r->price : NAN);
    }
    std::cout << '\n';
  }
  return 0;
}
