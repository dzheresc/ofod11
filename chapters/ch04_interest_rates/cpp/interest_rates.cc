// Copyright 2026 Denis Zhereshchin
//
// Chapter 4: compounding, bond pricing, bootstrapping, forward rates and
// duration, reproducing Tables 4.1, 4.3-4.6.

#include <format>
#include <iostream>
#include <vector>

#include "ofod/core/compounding.h"
#include "ofod/curves/bond_bootstrap.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/pricers/bond_pricer.h"

int main() {
  using ofod::Compounding;
  using ofod::curves::BondQuote;
  using ofod::instruments::Bond;
  namespace pricers = ofod::pricers;

  std::cout << "Table 4.1: $100 at 10% for one year\n";
  for (int m : {1, 2, 4, 12, 52, 365}) {
    std::cout << std::format(
        "  m = {:3d}: {:.2f}\n", m,
        100.0 * ofod::GrowthFactor(0.10, 1.0, Compounding::Periodic(m)));
  }

  const auto curve = *ofod::curves::ZeroCurve::Create(
      {0.5, 1.0, 1.5, 2.0}, {0.050, 0.058, 0.064, 0.068});
  const Bond two_year{.coupon_rate = 0.06, .maturity = 2.0};
  const double price = pricers::BondPrice(two_year, curve);
  std::cout << std::format(
      "\nTable 4.2 curve, 2-year 6% bond: price {:.2f}, yield {:.4f}, "
      "par yield {:.4f}\n",
      price, *pricers::BondYield(two_year, price),
      pricers::ParYield(2.0, 2, curve));

  const std::vector<BondQuote> quotes = {
      {Bond{.maturity = 0.25, .frequency = 4}, 97.5},
      {Bond{.maturity = 0.50}, 94.9},
      {Bond{.maturity = 1.00, .frequency = 1}, 90.0},
      {Bond{.coupon_rate = 0.08, .maturity = 1.50}, 96.0},
      {Bond{.coupon_rate = 0.12, .maturity = 2.00}, 101.6},
  };
  const auto boot = *ofod::curves::BootstrapFromBonds(quotes);
  std::cout << "\nTable 4.4: bootstrapped zero rates (continuous)\n";
  for (std::size_t i = 0; i < boot.times().size(); ++i) {
    std::cout << std::format("  {:.2f} y: {:.3f}%\n", boot.times()[i],
                             100.0 * boot.zero_rates()[i]);
  }

  const auto t45 = *ofod::curves::ZeroCurve::Create(
      {1, 2, 3, 4, 5}, {0.030, 0.040, 0.046, 0.050, 0.053});
  std::cout << "\nTable 4.5: forward rates\n";
  for (int n = 2; n <= 5; ++n) {
    std::cout << std::format("  year {}: {:.1f}%\n", n,
                             100.0 * t45.ForwardRate(n - 1, n));
  }

  const Bond three_year{.coupon_rate = 0.10, .maturity = 3.0};
  std::cout << std::format(
      "\nTable 4.6: 3-year 10% bond at 12%: price {:.3f}, duration {:.3f}, "
      "convexity {:.3f}\n",
      pricers::BondPriceFromYield(three_year, 0.12),
      pricers::MacaulayDuration(three_year, 0.12),
      pricers::Convexity(three_year, 0.12));
  return 0;
}
