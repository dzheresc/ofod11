// Copyright 2026 Denis Zhereshchin
//
// Chapter 16: valuing employee stock options with the Hull-White binomial
// model, the expected-life approach, and dilution.

#include "ofod/pricers/eso.h"

#include <format>
#include <iostream>
#include <string>

#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"

int main() {
  namespace p = ofod::pricers;
  const p::TreeMarket market{
      .spot = 40.0, .rate = 0.05, .yield = 0.0, .volatility = 0.30};
  std::cout << "Hull-White ESO, S0 = K = 40, 10 years, vesting 3 years, "
               "r = 5%, sigma = 30%\n";
  std::cout << "  multiple M    exit 0%   exit 4%   exit 8%\n";
  for (double m : {1.2, 1.5, 2.0, 3.0, 1e12}) {
    std::cout << (m > 1e6 ? std::string("  never   ")
                          : std::format("  {:<8.1f}", m));
    for (double exit_rate : {0.0, 0.04, 0.08}) {
      const auto v = p::PriceEmployeeStockOption({.strike = 40.0,
                                                  .maturity = 10.0,
                                                  .vesting = 3.0,
                                                  .exercise_multiple = m,
                                                  .exit_rate = exit_rate},
                                                 market, 1000);
      if (v) std::cout << std::format("{:>10.2f}", *v);
    }
    std::cout << '\n';
  }
  std::cout << std::format("European 10-year call (BSM): {:.2f}\n",
                           p::BsmPrice(ofod::instruments::OptionType::kCall,
                                       40.0, 40.0, 0.05, 0.0, 0.30, 10.0));
  std::cout << "Expected-life approach\n";
  for (double life : {4.0, 6.5, 10.0}) {
    std::cout << std::format(
        "  expected life {:>4.1f} years: {:.2f}\n", life,
        p::ExpectedLifeValue(40.0, 40.0, 0.05, 0.0, 0.30, life));
  }
  std::cout << std::format(
      "Dilution: 200,000 options at K = 60 on 1,000,000 shares at 40 are "
      "worth {:.2f} each\n",
      p::WarrantPrice(40.0, 60.0, 0.03, 0.30, 5.0, 1'000'000, 200'000));
  return 0;
}
