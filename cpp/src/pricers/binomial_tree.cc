// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/binomial_tree.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "ofod/pricers/black_scholes.h"

namespace ofod::pricers {

using instruments::Exercise;
using instruments::IntrinsicValue;
using instruments::VanillaOption;

namespace {

// factor^0, ..., factor^n via std::pow, so node prices match the direct
// formula bit for bit while costing O(n) pow calls instead of O(n^2).
std::vector<double> Powers(double factor, std::size_t n) {
  std::vector<double> out(n + 1);
  for (std::size_t k = 0; k <= n; ++k) {
    out[k] = std::pow(factor, static_cast<double>(k));
  }
  return out;
}

}  // namespace

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
  const std::vector<double> up_pow = Powers(u, n);
  const std::vector<double> down_pow = Powers(d, n);
  std::vector<double> values(n + 1);
  for (std::size_t j = 0; j <= n; ++j) {
    values[j] = IntrinsicValue(option, spot * up_pow[j] * down_pow[n - j]);
  }
  double value_up = 0.0;
  double value_down = 0.0;
  for (std::size_t i = n; i-- > 0;) {
    for (std::size_t j = 0; j <= i; ++j) {
      double v = disc * (p * values[j + 1] + (1.0 - p) * values[j]);
      if (american) {
        v = std::max(
            v, IntrinsicValue(option, spot * up_pow[j] * down_pow[i - j]));
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

std::optional<TreeResult> PriceBinomialWithDividends(
    const VanillaOption& option, const TreeMarket& market, int steps,
    std::span<const instruments::CashFlow> dividends) {
  if (steps < 1 || market.yield != 0.0) return std::nullopt;
  const double t = option.maturity;
  const double dt = t / steps;
  // PV at time `now` of the dividends paid in (now, T].
  auto remaining_pv = [&](double now) {
    double pv = 0.0;
    for (const auto& d : dividends) {
      if (d.time > now && d.time <= t) {
        pv += d.amount * std::exp(-market.rate * (d.time - now));
      }
    }
    return pv;
  };
  const double s_star = market.spot - remaining_pv(0.0);
  const TreeStep step =
      CoxRossRubinstein::Step(market.volatility, market.rate, dt);
  const double u = step.up;
  const double d = step.down;
  const double p = step.probability;
  const double disc = std::exp(-market.rate * dt);
  const bool american = option.exercise == Exercise::kAmerican;
  const auto n = static_cast<std::size_t>(steps);
  const std::vector<double> up_pow = Powers(u, n);
  const std::vector<double> down_pow = Powers(d, n);
  std::vector<double> pv_at(n + 1);
  for (std::size_t i = 0; i <= n; ++i) {
    pv_at[i] = remaining_pv(static_cast<double>(i) * dt);
  }
  auto node = [&](std::size_t i, std::size_t j) {
    return s_star * up_pow[j] * down_pow[i - j] + pv_at[i];
  };
  std::vector<double> values(n + 1);
  for (std::size_t j = 0; j <= n; ++j) {
    values[j] = IntrinsicValue(option, node(n, j));
  }
  double value_up = 0.0;
  double value_down = 0.0;
  for (std::size_t i = n; i-- > 0;) {
    for (std::size_t j = 0; j <= i; ++j) {
      double v = disc * (p * values[j + 1] + (1.0 - p) * values[j]);
      if (american) v = std::max(v, IntrinsicValue(option, node(i, j)));
      values[j] = v;
    }
    if (i == 1) {
      value_down = values[0];
      value_up = values[1];
    }
  }
  if (n == 1) {
    value_down = IntrinsicValue(option, node(1, 0));
    value_up = IntrinsicValue(option, node(1, 1));
  }
  const double delta = (value_up - value_down) / (node(1, 1) - node(1, 0));
  return TreeResult{values[0], delta};
}

std::optional<double> ControlVariateAmericanPrice(const VanillaOption& option,
                                                  const TreeMarket& market,
                                                  int steps) {
  VanillaOption american = option;
  american.exercise = Exercise::kAmerican;
  VanillaOption european = option;
  european.exercise = Exercise::kEuropean;
  const auto a = PriceBinomial<CoxRossRubinstein>(american, market, steps);
  const auto e = PriceBinomial<CoxRossRubinstein>(european, market, steps);
  if (!a || !e) return std::nullopt;
  const double bsm =
      BsmPrice(option.option_type, market.spot, option.strike, market.rate,
               market.yield, market.volatility, option.maturity);
  return a->price + bsm - e->price;
}

std::optional<TreeResult> PriceTrinomial(const VanillaOption& option,
                                         const TreeMarket& market, int steps) {
  if (steps < 1 || !(market.volatility > 0.0)) return std::nullopt;
  const double dt = option.maturity / steps;
  const double sigma = market.volatility;
  const double u = std::exp(sigma * std::sqrt(3.0 * dt));
  const double nu = market.rate - market.yield - 0.5 * sigma * sigma;
  const double tilt = nu * std::sqrt(dt / (12.0 * sigma * sigma));
  const double pu = 1.0 / 6.0 + tilt;
  const double pd = 1.0 / 6.0 - tilt;
  const double pm = 2.0 / 3.0;
  if (pu < 0.0 || pd < 0.0) return std::nullopt;
  const double disc = std::exp(-market.rate * dt);
  const bool american = option.exercise == Exercise::kAmerican;
  const int n = steps;
  // Node k in [-i, i] at step i sits at S0 u^k; stored at index k + i.
  const auto index = [](int i) { return static_cast<std::size_t>(i); };
  std::vector<double> level(index(2 * n + 1));
  for (int k = -n; k <= n; ++k) {
    level[index(k + n)] = market.spot * std::pow(u, static_cast<double>(k));
  }
  auto spot_at = [&](int k) { return level[index(k + n)]; };
  std::vector<double> values(index(2 * n + 1));
  for (int k = -n; k <= n; ++k) {
    values[index(k + n)] = IntrinsicValue(option, spot_at(k));
  }
  double value_up = 0.0;
  double value_down = 0.0;
  for (int i = n - 1; i >= 0; --i) {
    for (int k = -i; k <= i; ++k) {
      // Children of (i, k) are at k - 1, k, k + 1 in step i + 1, stored at
      // indices k + i, k + i + 1, k + i + 2.
      const auto base = index(k + i);
      double v = disc * (pd * values[base] + pm * values[base + 1] +
                         pu * values[base + 2]);
      if (american) v = std::max(v, IntrinsicValue(option, spot_at(k)));
      values[base] = v;
    }
    if (i == 1) {
      value_down = values[0];
      value_up = values[2];
    }
  }
  if (n == 1) {
    value_down = IntrinsicValue(option, spot_at(-1));
    value_up = IntrinsicValue(option, spot_at(1));
  }
  const double delta = (value_up - value_down) / (spot_at(1) - spot_at(-1));
  return TreeResult{values[0], delta};
}

}  // namespace ofod::pricers
