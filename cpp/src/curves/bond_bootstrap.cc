// Copyright 2026 Denis Zhereshchin

#include "ofod/curves/bond_bootstrap.h"

#include <algorithm>
#include <vector>

#include "ofod/math/roots.h"
#include "ofod/pricers/bond_pricer.h"

namespace ofod::curves {

std::optional<ZeroCurve> BootstrapFromBonds(std::span<const BondQuote> quotes) {
  std::vector<BondQuote> sorted(quotes.begin(), quotes.end());
  std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
    return a.bond.maturity < b.bond.maturity;
  });

  std::vector<double> times;
  std::vector<double> rates;
  for (const auto& quote : sorted) {
    const double maturity = quote.bond.maturity;
    if (!times.empty() && maturity <= times.back()) return std::nullopt;
    auto reprice_error = [&](double rate) {
      std::vector<double> trial_times = times;
      std::vector<double> trial_rates = rates;
      trial_times.push_back(maturity);
      trial_rates.push_back(rate);
      const auto trial =
          ZeroCurve::Create(std::move(trial_times), std::move(trial_rates));
      return pricers::BondPrice(quote.bond, *trial) - quote.price;
    };
    const auto rate = math::Brent(reprice_error, -0.5, 2.0);
    if (!rate) return std::nullopt;
    times.push_back(maturity);
    rates.push_back(*rate);
  }
  return ZeroCurve::Create(std::move(times), std::move(rates));
}

}  // namespace ofod::curves
