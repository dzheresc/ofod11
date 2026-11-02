// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/binomial_tree.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ofod::pricers {

using instruments::Exercise;
using instruments::IntrinsicValue;
using instruments::VanillaOption;

double RiskNeutralProbability(double up, double down, double growth) {
  return (growth - down) / (up - down);
}

TreeStep CoxRossRubinstein::Step(double volatility, double drift, double dt) {
  const double up = std::exp(volatility * std::sqrt(dt));
  const double down = 1.0 / up;
  return {up, down, RiskNeutralProbability(up, down, std::exp(drift * dt))};
}

TreeStep JarrowRudd::Step(double volatility, double drift, double dt) {
  const double mu = (drift - 0.5 * volatility * volatility) * dt;
  const double jump = volatility * std::sqrt(dt);
  return {std::exp(mu + jump), std::exp(mu - jump), 0.5};
}

TreeStep Tian::Step(double volatility, double drift, double dt) {
  const double a = std::exp(drift * dt);
  const double v = std::exp(volatility * volatility * dt);
  const double root = std::sqrt(v * v + 2.0 * v - 3.0);
  const double up = 0.5 * a * v * (v + 1.0 + root);
  const double down = 0.5 * a * v * (v + 1.0 - root);
  return {up, down, RiskNeutralProbability(up, down, a)};
}

TreeStep SchemeStep(TreeScheme scheme, double volatility, double drift,
                    double dt) {
  switch (scheme) {
    case TreeScheme::kCoxRossRubinstein:
      return CoxRossRubinstein::Step(volatility, drift, dt);
    case TreeScheme::kJarrowRudd:
      return JarrowRudd::Step(volatility, drift, dt);
    case TreeScheme::kTian:
      return Tian::Step(volatility, drift, dt);
  }
  return CoxRossRubinstein::Step(volatility, drift, dt);
}

std::optional<TreeResult> PriceTree(const VanillaOption& option, double spot,
                                    double rate, int steps,
                                    const TreeStep& step) {
  const double u = step.up;
  const double d = step.down;
  const double p = step.probability;
  if (steps < 1 || !(u > d) || !(p >= 0.0 && p <= 1.0)) return std::nullopt;
  const double dt = option.maturity / steps;
  const double disc = std::exp(-rate * dt);
  const bool american = option.exercise == Exercise::kAmerican;
  const auto n = static_cast<std::size_t>(steps);

  // values[j] holds the node with j up moves at the current step.
  std::vector<double> values(n + 1);
  for (std::size_t j = 0; j <= n; ++j) {
    const double s = spot * std::pow(u, static_cast<double>(j)) *
                     std::pow(d, static_cast<double>(n - j));
    values[j] = IntrinsicValue(option, s);
  }
  double value_up = 0.0;
  double value_down = 0.0;
  for (std::size_t i = n; i-- > 0;) {
    for (std::size_t j = 0; j <= i; ++j) {
      double v = disc * (p * values[j + 1] + (1.0 - p) * values[j]);
      if (american) {
        const double s = spot * std::pow(u, static_cast<double>(j)) *
                         std::pow(d, static_cast<double>(i - j));
        v = std::max(v, IntrinsicValue(option, s));
      }
      values[j] = v;
    }
    if (i == 1) {
      value_down = values[0];
      value_up = values[1];
    }
  }
  if (n == 1) {
    // The first-step values are the payoffs themselves.
    value_down = IntrinsicValue(option, spot * d);
    value_up = IntrinsicValue(option, spot * u);
  }
  const double delta = (value_up - value_down) / (spot * u - spot * d);
  return TreeResult{values[0], delta};
}

std::optional<TreeResult> PriceBinomial(const VanillaOption& option,
                                        const TreeMarket& market, int steps,
                                        TreeScheme scheme) {
  if (steps < 1) return std::nullopt;
  const double dt = option.maturity / steps;
  return PriceTree(
      option, market.spot, market.rate, steps,
      SchemeStep(scheme, market.volatility, market.rate - market.yield, dt));
}

}  // namespace ofod::pricers
