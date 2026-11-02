// Copyright 2026 Denis Zhereshchin
//
// Chapter 27: smiles implied by CEV, jump-diffusion and Heston prices,
// Heston by Monte Carlo, Longstaff-Schwartz against a binomial tree and
// Hull's convertible bond example.

#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <limits>

#include "ofod/pricers/american_mc.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/model_pricers.h"

namespace {

namespace p = ofod::pricers;
using ofod::instruments::Exercise;
using ofod::instruments::VanillaOption;
using p::OptionType;
using p::TreeMarket;

double ImpliedVol(double price, double strike) {
  const OptionType type = strike < 100.0 ? OptionType::kPut : OptionType::kCall;
  return p::ImpliedVolatility(type, price, 100.0, strike, 0.05, 0.0, 1.0)
      .value_or(0.0);
}

void Smiles() {
  // All three models are set up near a 25% at-the-money volatility.
  const TreeMarket cev{100.0, 0.05, 0.0, 0.25 * 10.0};
  const TreeMarket jump{100.0, 0.05, 0.0, 0.2};
  const p::JumpParams jumps{
      .intensity = 0.5, .mean_jump = -0.1, .jump_vol = 0.15};
  const TreeMarket heston{100.0, 0.05, 0.0, 0.0};
  const p::HestonParams hp{
      .v0 = 0.0625, .kappa = 2.0, .theta = 0.0625, .sigma = 0.5, .rho = -0.7};
  std::cout << "Implied volatility smiles (S = 100, r = 5%, T = 1)\n";
  std::cout << "  strike   CEV a=0.5   Merton jump   Heston\n";
  for (double k = 70.0; k <= 130.0; k += 10.0) {
    const OptionType type = k < 100.0 ? OptionType::kPut : OptionType::kCall;
    const double c = p::CevPrice(type, cev, k, 1.0, 0.5).value_or(0.0);
    const double j = p::MertonJumpPrice(type, jump, k, 1.0, jumps);
    const double h = p::HestonPrice(type, heston, k, 1.0, hp);
    std::cout << std::format("  {:6.0f}  {:9.2f}%  {:11.2f}%  {:6.2f}%\n", k,
                             100.0 * ImpliedVol(c, k), 100.0 * ImpliedVol(j, k),
                             100.0 * ImpliedVol(h, k));
  }
  const double exact =
      p::HestonPrice(OptionType::kCall, heston, 100.0, 1.0, hp);
  const auto mc = p::HestonMonteCarlo(OptionType::kCall, heston, 100.0, 1.0, hp,
                                      100, 50'000, 1);
  if (!mc) return;
  std::cout << std::format(
      "Heston ATM call: semi-analytic {:.4f}, Monte Carlo {:.4f} (se "
      "{:.4f})\n",
      exact, mc->price, mc->std_error);
}

void American() {
  const TreeMarket m{36.0, 0.06, 0.0, 0.2};
  const VanillaOption put{.option_type = OptionType::kPut,
                          .exercise = Exercise::kAmerican,
                          .strike = 40.0,
                          .maturity = 1.0};
  const auto tree =
      p::PriceBinomial(put, m, 2000, p::TreeScheme::kCoxRossRubinstein);
  const auto lsm =
      p::LongstaffSchwartz(OptionType::kPut, m, 40.0, 1.0, {.paths = 50'000});
  if (!tree || !lsm) return;
  std::cout << std::format(
      "American put, S = 36, K = 40 (Longstaff-Schwartz 2001): tree {:.4f}, "
      "LSM {:.4f} (se {:.4f}), European {:.4f}\n",
      tree->price, lsm->price, lsm->std_error,
      p::BsmPrice(OptionType::kPut, 36.0, 40.0, 0.06, 0.0, 0.2, 1.0));
}

void Convertible() {
  const TreeMarket m{50.0, 0.05, 0.0, 0.3};
  p::ConvertibleTerms terms{.face = 100.0,
                            .conversion_ratio = 2.0,
                            .maturity = 0.75,
                            .call_price = 113.0,
                            .recovery = 0.4,
                            .hazard = 0.01};
  std::cout << "Convertible (Example 27.1): 9 months, 2 shares, callable at "
               "113\n";
  for (int steps : {3, 30, 300}) {
    std::cout << std::format(
        "  {:3d} steps: {:.4f}\n", steps,
        p::ConvertibleBondPrice(m, terms, steps).value_or(0.0));
  }
  terms.call_price = std::numeric_limits<double>::infinity();
  std::cout << std::format(
      "  non-callable, 300 steps: {:.4f}\n",
      p::ConvertibleBondPrice(m, terms, 300).value_or(0.0));
}

}  // namespace

int main() {
  try {
    Smiles();
    American();
    Convertible();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
