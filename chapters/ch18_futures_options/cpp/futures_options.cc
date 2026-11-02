// Copyright 2026 Denis Zhereshchin
//
// Chapter 18: Black's model, put-call parity for futures options, futures
// options on a tree, and futures-style options.

#include <cmath>
#include <format>
#include <iostream>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"

int main() {
  namespace p = ofod::pricers;
  using ofod::instruments::Exercise;
  using ofod::instruments::OptionType;

  const double t = 4.0 / 12;
  const double put = p::Black76Price(OptionType::kPut, 20, 20, 0.09, 0.25, t);
  const double call = p::Black76Price(OptionType::kCall, 20, 20, 0.09, 0.25, t);
  std::cout << std::format("Crude oil futures put (Black): {:.2f}\n", put);
  std::cout << std::format("Parity: put from call {:.4f} vs Black {:.4f}\n",
                           p::FuturesPutFromParity(call, 20, 20, 0.09, t), put);

  ofod::instruments::VanillaOption option{
      .option_type = OptionType::kPut, .strike = 20, .maturity = t};
  const p::TreeMarket futures{
      .spot = 20, .rate = 0.09, .yield = 0.09, .volatility = 0.25};
  const auto european =
      p::PriceBinomial<p::CoxRossRubinstein>(option, futures, 1000);
  option.exercise = Exercise::kAmerican;
  const auto american =
      p::PriceBinomial<p::CoxRossRubinstein>(option, futures, 1000);
  if (!european || !american) return 1;
  std::cout << std::format(
      "Tree with q = r: European {:.4f}, American {:.4f}\n", european->price,
      american->price);

  const double spot = 20.0 * std::exp(-0.09 * t);
  std::cout << std::format(
      "Black on F = S e^(rT) equals BSM on S: {:.6f} vs {:.6f}\n", call,
      p::BsmPrice(OptionType::kCall, spot, 20, 0.09, 0.0, 0.25, t));
  std::cout << std::format(
      "Futures-style call (no discounting): {:.4f}\n",
      p::FuturesStyleOptionPrice(OptionType::kCall, 20, 20, 0.25, t));
  return 0;
}
