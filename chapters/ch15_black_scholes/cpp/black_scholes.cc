// Copyright 2026 Denis Zhereshchin
//
// Chapter 15: lognormal property, historical volatility, BSM prices,
// implied volatility, dividends, Black's approximation and warrants.

#include "ofod/pricers/black_scholes.h"

#include <cmath>
#include <format>
#include <iostream>
#include <vector>

#include "ofod/instruments/cash_flow.h"
#include "ofod/math/normal.h"
#include "ofod/models/brownian.h"
#include "ofod/stats/descriptive.h"

int main() {
  namespace p = ofod::pricers;
  using ofod::instruments::OptionType;

  const ofod::models::GeometricBrownianMotion gbm{.drift = 0.16,
                                                  .volatility = 0.20};
  const auto ci = gbm.ConfidenceInterval(40.0, 0.5, 1.96);
  std::cout << std::format(
      "S0=40, mu=16%, sigma=20%, T=0.5: 95% range {:.2f}-{:.2f}, "
      "E={:.2f}, sd={:.2f}\n",
      ci.lower, ci.upper, gbm.ExpectedValue(40.0, 0.5),
      std::sqrt(gbm.Variance(40.0, 0.5)));

  const std::vector<double> closes = {20.00, 20.10, 19.90, 20.00, 20.50, 20.25,
                                      20.90, 20.90, 20.90, 20.75, 20.75, 21.00,
                                      21.10, 20.90, 20.90, 21.25, 21.40, 21.40,
                                      21.25, 21.75, 22.00};
  if (const auto vol = ofod::stats::HistoricalVolatility(closes, 252)) {
    std::cout << std::format("Table 15.1 historical volatility: {:.1f}%\n",
                             100.0 * *vol);
  }

  std::cout << std::format(
      "N(0.7693) = {:.4f} (erfc) vs {:.4f} (Hull's polynomial)\n",
      ofod::math::NormCdf(0.7693), ofod::math::NormCdfPoly(0.7693));
  std::cout << std::format(
      "Example 15.6: c = {:.2f}, p = {:.2f}\n",
      p::BsmPrice(OptionType::kCall, 42, 40, 0.10, 0.0, 0.20, 0.5),
      p::BsmPrice(OptionType::kPut, 42, 40, 0.10, 0.0, 0.20, 0.5));
  if (const auto iv = p::ImpliedVolatility(OptionType::kCall, 1.875, 21, 20,
                                           0.10, 0.0, 0.25)) {
    std::cout << std::format("Implied vol of c = 1.875: {:.2f}%\n",
                             100.0 * *iv);
  }

  const std::vector<ofod::instruments::CashFlow> divs = {{2.0 / 12, 0.5},
                                                         {5.0 / 12, 0.5}};
  std::cout << std::format(
      "Dividends: European call {:.2f}, Black's American approximation "
      "{:.2f}\n",
      p::BsmPriceWithDividends(OptionType::kCall, 40, 40, 0.09, 0.30, 0.5,
                               divs),
      p::BlackAmericanCallApproximation(40, 40, 0.09, 0.30, 0.5, divs));
  std::cout << std::format(
      "Warrant: {:.2f}\n",
      p::WarrantPrice(40, 60, 0.03, 0.30, 5.0, 1'000'000, 200'000));
  return 0;
}
