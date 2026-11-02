// Copyright 2026 Denis Zhereshchin
//
// The Greek letters (Hull, Ch 19): analytic BSM Greeks, a finite-difference
// bumper that works with any pricer, delta-gamma-vega neutral hedges and
// scenario grids. Theta is per year, vega per unit of volatility and rho per
// unit of rate (divide by 365, 100 and 100 for Hull's per-day / per-1%).

#ifndef OFOD_GREEKS_GREEKS_H_
#define OFOD_GREEKS_GREEKS_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "ofod/instruments/option.h"

namespace ofod::greeks {

// Market inputs a pricer is bumped against; strike and option type live in
// the pricer itself.
struct OptionInputs {
  double spot = 0.0;
  double rate = 0.0;
  double yield = 0.0;
  double volatility = 0.0;
  double maturity = 0.0;
};

struct Greeks {
  double delta;
  double gamma;
  double vega;
  double theta;
  double rho;
};

// Closed forms for a European option on an asset with yield q (Table 19.6).
[[nodiscard]] Greeks BsmGreeks(instruments::OptionType type, double strike,
                               const OptionInputs& in);

// Bump sizes: `spot` is relative to S0, the rest absolute.
struct BumpSizes {
  double spot = 1e-3;
  double volatility = 1e-4;
  double rate = 1e-4;
  double time = 1.0 / 365.0;
};

// Central differences of `pricer(OptionInputs) -> double`. Theta is
// -dV/dT, one-sided when the maturity is shorter than the time bump.
template <class Pricer>
[[nodiscard]] Greeks BumpGreeks(Pricer&& pricer, const OptionInputs& in,
                                const BumpSizes& bumps = {}) {
  auto with = [&in](auto mutate) {
    OptionInputs x = in;
    mutate(x);
    return x;
  };
  const double v0 = pricer(in);
  const double hs = in.spot * bumps.spot;
  const double up = pricer(with([&](OptionInputs& x) { x.spot += hs; }));
  const double down = pricer(with([&](OptionInputs& x) { x.spot -= hs; }));
  const double hv = bumps.volatility;
  const double vega =
      (pricer(with([&](OptionInputs& x) { x.volatility += hv; })) -
       pricer(with([&](OptionInputs& x) { x.volatility -= hv; }))) /
      (2.0 * hv);
  const double hr = bumps.rate;
  const double rho = (pricer(with([&](OptionInputs& x) { x.rate += hr; })) -
                      pricer(with([&](OptionInputs& x) { x.rate -= hr; }))) /
                     (2.0 * hr);
  const double ht = bumps.time;
  const double longer =
      pricer(with([&](OptionInputs& x) { x.maturity += ht; }));
  const double theta =
      in.maturity > ht
          ? (pricer(with([&](OptionInputs& x) { x.maturity -= ht; })) -
             longer) /
                (2.0 * ht)
          : (v0 - longer) / ht;
  return {.delta = (up - down) / (2.0 * hs),
          .gamma = (up - 2.0 * v0 + down) / (hs * hs),
          .vega = vega,
          .theta = theta,
          .rho = rho};
}

using Pricer = std::function<double(const OptionInputs&)>;

[[nodiscard]] Greeks BumpGreeksErased(const Pricer& pricer,
                                      const OptionInputs& in,
                                      const BumpSizes& bumps = {});

// Delta, gamma and vega of one position or one unit of a traded option.
struct Exposure {
  double delta = 0.0;
  double gamma = 0.0;
  double vega = 0.0;
};

// Units of the traded options and of the underlying that make the combined
// position neutral (Ch 19.10). `option2` is 0 for a gamma-only hedge.
struct NeutralHedge {
  double option1;
  double option2;
  double underlying;
};

// Gamma neutral with one option, then delta neutral with the underlying.
// Returns std::nullopt if the option has no gamma.
[[nodiscard]] std::optional<NeutralHedge> GammaNeutralHedge(
    const Exposure& portfolio, const Exposure& option);

// Gamma and vega neutral with two options, then delta neutral. Returns
// std::nullopt if the options' (gamma, vega) vectors are parallel.
[[nodiscard]] std::optional<NeutralHedge> GammaVegaNeutralHedge(
    const Exposure& portfolio, const Exposure& option1,
    const Exposure& option2);

// Change in value of `pricer` for each (volatility, spot) pair (Table 19.8
// layout): row i uses volatilities[i], column j uses spots[j].
[[nodiscard]] std::vector<std::vector<double>> ScenarioGrid(
    const Pricer& pricer, const OptionInputs& base,
    std::span<const double> spots, std::span<const double> volatilities);

// The delta-hedging experiment behind Hull's hedging-performance table
// (Ch 19.4): write one call, delta hedge at `rebalances` equally spaced
// dates under real-world drift `drift`, and record the discounted cost.
struct HedgeExperiment {
  double spot = 0.0;
  double strike = 0.0;
  double rate = 0.0;
  double drift = 0.0;
  double volatility = 0.0;
  double maturity = 0.0;
};

struct HedgePerformance {
  double mean_cost;
  double std_cost;
  double bsm_price;
  double ratio;  // std_cost / bsm_price, Hull's performance measure.
};

// Returns std::nullopt if rebalances < 1 or paths < 2.
[[nodiscard]] std::optional<HedgePerformance> SimulateDeltaHedge(
    const HedgeExperiment& experiment, int rebalances, int paths,
    std::uint64_t seed);

}  // namespace ofod::greeks

#endif  // OFOD_GREEKS_GREEKS_H_
