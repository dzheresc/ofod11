// Copyright 2026 Denis Zhereshchin
//
// Chapter 17: index options with a dividend yield, currency options
// (Garman-Kohlhagen), portfolio insurance, and American options on trees.

#include <format>
#include <iostream>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/risk/hedging.h"

int main() {
  namespace p = ofod::pricers;
  using ofod::instruments::Exercise;
  using ofod::instruments::OptionType;

  std::cout << std::format(
      "Example 17.1 index call: {:.2f}\n",
      p::BsmPrice(OptionType::kCall, 930, 900, 0.08, 0.03, 0.20, 2.0 / 12));
  std::cout << std::format(
      "GBP call (S=1.6, K=1.6, r=8%, rf=11%, sigma=14.1%, 4m): {:.4f}\n",
      p::GarmanKohlhagenPrice(OptionType::kCall, 1.6, 1.6, 0.08, 0.11, 0.141,
                              4.0 / 12));
  std::cout << std::format(
      "Portfolio insurance: beta 2.0, $500,000, index 1000 x 100 -> {:.0f} "
      "put contracts\n",
      ofod::risk::BetaHedgeContracts(2.0, 500'000, 1000.0 * 100));

  const p::TreeMarket fx{
      .spot = 1.6, .rate = 0.08, .yield = 0.11, .volatility = 0.141};
  ofod::instruments::VanillaOption call{.strike = 1.6, .maturity = 1.0};
  const auto european =
      p::PriceBinomial(call, fx, 500, p::TreeScheme::kCoxRossRubinstein);
  call.exercise = Exercise::kAmerican;
  const auto american =
      p::PriceBinomial(call, fx, 500, p::TreeScheme::kCoxRossRubinstein);
  if (!european || !american) return 1;
  std::cout << std::format(
      "1y GBP call on a tree: European {:.4f}, American {:.4f} (rf > r makes "
      "early exercise valuable)\n",
      european->price, american->price);
  return 0;
}
