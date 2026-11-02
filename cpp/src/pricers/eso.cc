// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/eso.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"

namespace ofod::pricers {

std::optional<double> PriceEmployeeStockOption(const EsoTerms& terms,
                                               const TreeMarket& market,
                                               int steps) {
  if (steps < 1 || terms.exercise_multiple < 1.0 || terms.exit_rate < 0.0 ||
      terms.vesting < 0.0 || terms.vesting > terms.maturity) {
    return std::nullopt;
  }
  const double dt = terms.maturity / steps;
  const TreeStep step = CoxRossRubinstein::Step(market.volatility,
                                                market.rate - market.yield, dt);
  if (!(step.probability >= 0.0 && step.probability <= 1.0)) {
    return std::nullopt;
  }
  const double disc = std::exp(-market.rate * dt);
  const double leave = 1.0 - std::exp(-terms.exit_rate * dt);
  const double k = terms.strike;
  const double trigger = terms.exercise_multiple * k;
  // Vested from step `first_vested` on (tolerating rounding in i * dt).
  const int first_vested =
      static_cast<int>(std::ceil(terms.vesting / dt - 1e-9));
  const auto index = [](int i) { return static_cast<std::size_t>(i); };

  std::vector<double> values(index(steps + 1));
  for (int j = 0; j <= steps; ++j) {
    const double s =
        market.spot * std::pow(step.up, j) * std::pow(step.down, steps - j);
    values[index(j)] = steps >= first_vested ? std::max(s - k, 0.0) : 0.0;
  }
  for (int i = steps - 1; i >= 0; --i) {
    const bool vested = i >= first_vested;
    for (int j = 0; j <= i; ++j) {
      const double s =
          market.spot * std::pow(step.up, j) * std::pow(step.down, i - j);
      const double hold = disc * (step.probability * values[index(j + 1)] +
                                  (1.0 - step.probability) * values[index(j)]);
      double v = 0.0;
      if (!vested) {
        v = (1.0 - leave) * hold;
      } else if (s >= trigger) {
        v = s - k;
      } else {
        v = (1.0 - leave) * hold + leave * std::max(s - k, 0.0);
      }
      values[index(j)] = v;
    }
  }
  return values[0];
}

double ExpectedLifeValue(double spot, double strike, double rate, double yield,
                         double volatility, double expected_life) {
  return BsmPrice(instruments::OptionType::kCall, spot, strike, rate, yield,
                  volatility, expected_life);
}

}  // namespace ofod::pricers
