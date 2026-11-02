// Copyright 2026 Denis Zhereshchin
//
// Monte Carlo valuation under geometric Brownian motion (Hull, Ch 21.6-21.7).

#ifndef OFOD_PRICERS_MONTE_CARLO_H_
#define OFOD_PRICERS_MONTE_CARLO_H_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/random/normal_generator.h"

namespace ofod::pricers {

enum class VarianceReduction {
  kNone,
  kAntithetic,          // Pair each draw z with -z.
  kControlVariate,      // S_T as control, its mean S0 e^{(r-q)T} known.
  kImportanceSampling,  // Draws shifted to centre S_T on an out-of-the-money
                        // strike (no shift when in the money).
  kStratified,          // One draw per equal-probability stratum.
  kMomentMatching,      // Draws rescaled to sample mean 0, variance 1.
  kQuasiRandom,         // Sobol points, randomly shifted.
};

struct McOptions {
  int paths = 100'000;
  std::uint64_t seed = 1;
  VarianceReduction method = VarianceReduction::kNone;
  int threads = 1;  // Path simulator only; results do not depend on it.
};

struct McResult {
  double price;
  double std_error;
  int paths;
};

// European option priced by sampling S_T directly. Methods whose draws are
// not independent (stratified, moment matching, quasi-random) estimate the
// standard error from 16 independent batches. Returns std::nullopt for
// American options or fewer than 32 paths.
[[nodiscard]] std::optional<McResult> MonteCarloEuropean(
    const instruments::VanillaOption& option, const TreeMarket& market,
    const McOptions& options);

// SplitMix64: derives independent stream seeds from (seed, chunk).
[[nodiscard]] std::uint64_t MixSeed(std::uint64_t seed, std::uint64_t chunk);

// Simulates `options.paths` risk-neutral GBM paths with `steps` steps over
// [0, t] and returns the discounted mean of `payoff(path)`, where `path`
// holds S at times 0, dt, ..., t. Paths are generated in chunks of
// kChunkPaths with seeds MixSeed(seed, chunk), so the result is identical
// for any number of threads. Supports kNone and kAntithetic.
inline constexpr int kChunkPaths = 1024;

template <class Payoff>
[[nodiscard]] std::optional<McResult> SimulateGbmPaths(
    const TreeMarket& market, double t, int steps, Payoff&& payoff,
    const McOptions& options) {
  if (steps < 1 || options.paths < 2 || options.threads < 1) {
    return std::nullopt;
  }
  if (options.method != VarianceReduction::kNone &&
      options.method != VarianceReduction::kAntithetic) {
    return std::nullopt;
  }
  const bool antithetic = options.method == VarianceReduction::kAntithetic;
  const double dt = t / steps;
  const double mu = (market.rate - market.yield -
                     0.5 * market.volatility * market.volatility) *
                    dt;
  const double jump = market.volatility * std::sqrt(dt);
  const int chunks = (options.paths + kChunkPaths - 1) / kChunkPaths;
  struct Sums {
    double sum = 0.0;
    double sum_sq = 0.0;
    int count = 0;
  };
  std::vector<Sums> per_chunk(static_cast<std::size_t>(chunks));

  auto run_chunk = [&](int c) {
    random::NormalGenerator gen(
        MixSeed(options.seed, static_cast<std::uint64_t>(c)));
    const int first = c * kChunkPaths;
    const int count = std::min(kChunkPaths, options.paths - first);
    std::vector<double> z(static_cast<std::size_t>(steps));
    std::vector<double> path(static_cast<std::size_t>(steps) + 1);
    Sums s;
    auto walk = [&](double sign) {
      path[0] = market.spot;
      for (std::size_t k = 0; k < z.size(); ++k) {
        path[k + 1] = path[k] * std::exp(mu + jump * sign * z[k]);
      }
      return static_cast<double>(payoff(std::span<const double>(path)));
    };
    for (int i = 0; i < count; ++i) {
      for (auto& x : z) x = gen.Next();
      double value = walk(1.0);
      if (antithetic) value = 0.5 * (value + walk(-1.0));
      s.sum += value;
      s.sum_sq += value * value;
      ++s.count;
    }
    per_chunk[static_cast<std::size_t>(c)] = s;
  };

  const int workers = std::min(options.threads, chunks);
  if (workers <= 1) {
    for (int c = 0; c < chunks; ++c) run_chunk(c);
  } else {
    std::vector<std::jthread> pool;
    pool.reserve(static_cast<std::size_t>(workers));
    for (int w = 0; w < workers; ++w) {
      pool.emplace_back([&, w] {
        for (int c = w; c < chunks; c += workers) run_chunk(c);
      });
    }
  }  // jthreads join here.

  Sums total;
  for (const auto& s : per_chunk) {
    total.sum += s.sum;
    total.sum_sq += s.sum_sq;
    total.count += s.count;
  }
  const double n = total.count;
  const double mean = total.sum / n;
  const double var =
      std::max((total.sum_sq - n * mean * mean) / (n - 1.0), 0.0);
  const double disc = std::exp(-market.rate * t);
  return McResult{disc * mean, disc * std::sqrt(var / n), total.count};
}

// Type-erased form of SimulateGbmPaths, used by the Python bindings.
[[nodiscard]] std::optional<McResult> SimulateGbmPathsErased(
    const TreeMarket& market, double t, int steps,
    const std::function<double(std::span<const double>)>& payoff,
    const McOptions& options);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_MONTE_CARLO_H_
