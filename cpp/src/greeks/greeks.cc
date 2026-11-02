// Copyright 2026 Denis Zhereshchin

#include "ofod/greeks/greeks.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ofod/instruments/option.h"
#include "ofod/math/normal.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/random/normal_generator.h"

namespace ofod::greeks {

using instruments::OptionType;
using math::NormCdf;
using math::NormPdf;

Greeks BsmGreeks(OptionType type, double strike, const OptionInputs& in) {
  const double s = in.spot;
  const double t = in.maturity;
  const double sqrt_t = std::sqrt(t);
  const double d1 =
      pricers::BsmD1(s, strike, in.rate, in.yield, in.volatility, t);
  const double d2 = d1 - in.volatility * sqrt_t;
  const double dq = std::exp(-in.yield * t);
  const double dr = std::exp(-in.rate * t);
  const double gamma = dq * NormPdf(d1) / (s * in.volatility * sqrt_t);
  const double vega = s * dq * NormPdf(d1) * sqrt_t;
  const double decay = -s * NormPdf(d1) * in.volatility * dq / (2.0 * sqrt_t);
  if (type == OptionType::kCall) {
    return {.delta = dq * NormCdf(d1),
            .gamma = gamma,
            .vega = vega,
            .theta = decay + in.yield * s * NormCdf(d1) * dq -
                     in.rate * strike * dr * NormCdf(d2),
            .rho = strike * t * dr * NormCdf(d2)};
  }
  return {.delta = dq * (NormCdf(d1) - 1.0),
          .gamma = gamma,
          .vega = vega,
          .theta = decay - in.yield * s * NormCdf(-d1) * dq +
                   in.rate * strike * dr * NormCdf(-d2),
          .rho = -strike * t * dr * NormCdf(-d2)};
}

Greeks BumpGreeksErased(const Pricer& pricer, const OptionInputs& in,
                        const BumpSizes& bumps) {
  return BumpGreeks(pricer, in, bumps);
}

std::optional<NeutralHedge> GammaNeutralHedge(const Exposure& portfolio,
                                              const Exposure& option) {
  if (option.gamma == 0.0) return std::nullopt;
  const double w = -portfolio.gamma / option.gamma;
  return NeutralHedge{.option1 = w,
                      .option2 = 0.0,
                      .underlying = -(portfolio.delta + w * option.delta)};
}

std::optional<NeutralHedge> GammaVegaNeutralHedge(const Exposure& portfolio,
                                                  const Exposure& option1,
                                                  const Exposure& option2) {
  // Solve [g1 g2; v1 v2] [w1; w2] = -[gP; vP] by Cramer's rule.
  const double det =
      option1.gamma * option2.vega - option2.gamma * option1.vega;
  if (std::abs(det) < 1e-14) return std::nullopt;
  const double w1 =
      (-portfolio.gamma * option2.vega + portfolio.vega * option2.gamma) / det;
  const double w2 =
      (-portfolio.vega * option1.gamma + portfolio.gamma * option1.vega) / det;
  return NeutralHedge{.option1 = w1,
                      .option2 = w2,
                      .underlying = -(portfolio.delta + w1 * option1.delta +
                                      w2 * option2.delta)};
}

std::vector<std::vector<double>> ScenarioGrid(
    const Pricer& pricer, const OptionInputs& base,
    std::span<const double> spots, std::span<const double> volatilities) {
  const double v0 = pricer(base);
  std::vector<std::vector<double>> grid;
  grid.reserve(volatilities.size());
  for (double vol : volatilities) {
    std::vector<double> row;
    row.reserve(spots.size());
    for (double spot : spots) {
      OptionInputs x = base;
      x.spot = spot;
      x.volatility = vol;
      row.push_back(pricer(x) - v0);
    }
    grid.push_back(std::move(row));
  }
  return grid;
}

std::optional<HedgePerformance> SimulateDeltaHedge(const HedgeExperiment& e,
                                                   int rebalances, int paths,
                                                   std::uint64_t seed) {
  if (rebalances < 1 || paths < 2) return std::nullopt;
  const double dt = e.maturity / rebalances;
  const double growth = std::exp(e.rate * dt);
  const double drift = (e.drift - 0.5 * e.volatility * e.volatility) * dt;
  const double diffusion = e.volatility * std::sqrt(dt);
  auto delta_at = [&e](double spot, double tau) {
    return NormCdf(
        pricers::BsmD1(spot, e.strike, e.rate, 0.0, e.volatility, tau));
  };
  random::NormalGenerator rng(seed);
  double sum = 0.0;
  double sum_sq = 0.0;
  for (int p = 0; p < paths; ++p) {
    double spot = e.spot;
    double shares = delta_at(spot, e.maturity);
    double cash = -shares * spot;
    for (int i = 1; i < rebalances; ++i) {
      spot *= std::exp(drift + diffusion * rng.Next());
      cash *= growth;
      const double target = delta_at(spot, e.maturity - i * dt);
      cash -= (target - shares) * spot;
      shares = target;
    }
    spot *= std::exp(drift + diffusion * rng.Next());
    cash *= growth;
    cash += shares * spot - std::max(spot - e.strike, 0.0);
    const double cost = -cash * std::exp(-e.rate * e.maturity);
    sum += cost;
    sum_sq += cost * cost;
  }
  const double n = paths;
  const double mean = sum / n;
  const double std_cost =
      std::sqrt(std::max(sum_sq / n - mean * mean, 0.0) * n / (n - 1.0));
  const double price = pricers::BsmPrice(OptionType::kCall, e.spot, e.strike,
                                         e.rate, 0.0, e.volatility, e.maturity);
  return HedgePerformance{.mean_cost = mean,
                          .std_cost = std_cost,
                          .bsm_price = price,
                          .ratio = std_cost / price};
}

}  // namespace ofod::greeks
