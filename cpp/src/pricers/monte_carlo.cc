// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/monte_carlo.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ofod/math/normal.h"
#include "ofod/random/sobol.h"

namespace ofod::pricers {

namespace {

using instruments::Exercise;
using instruments::IntrinsicValue;
using instruments::OptionType;
using instruments::VanillaOption;

constexpr int kBatches = 16;

struct Moments {
  double mean;
  double variance;  // Sample variance of the per-path estimates.
};

Moments SampleMoments(std::span<const double> x) {
  const auto n = static_cast<double>(x.size());
  double sum = 0.0;
  for (double v : x) sum += v;
  const double mean = sum / n;
  double ss = 0.0;
  for (double v : x) ss += (v - mean) * (v - mean);
  return {mean, ss / (n - 1.0)};
}

// Terminal price for a standard normal draw.
struct Terminal {
  double s0;
  double mu;   // (r - q - sigma^2/2) T.
  double vol;  // sigma sqrt(T).
  double operator()(double z) const { return s0 * std::exp(mu + vol * z); }
};

// Undiscounted payoffs from `n` normals of one batch.
std::vector<double> BatchNormals(VarianceReduction method, int n,
                                 std::uint64_t seed) {
  random::NormalGenerator gen(seed);
  std::vector<double> z(static_cast<std::size_t>(n));
  if (method == VarianceReduction::kStratified) {
    // Stratum i covers [i/n, (i+1)/n).
    std::mt19937_64 engine(seed);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    for (int i = 0; i < n; ++i) {
      const double u = (i + uniform(engine)) / n;
      z[static_cast<std::size_t>(i)] =
          math::NormInv(std::clamp(u, 1e-16, 1.0 - 1e-16));
    }
  } else if (method == VarianceReduction::kQuasiRandom) {
    // Sobol points with a Cranley-Patterson random shift per batch.
    auto sobol = random::Sobol::Create(1);
    std::mt19937_64 engine(seed);
    const double shift =
        std::uniform_real_distribution<double>(0.0, 1.0)(engine);
    for (int i = 0; i < n; ++i) {
      double u = sobol->Next()[0] + shift;
      if (u >= 1.0) u -= 1.0;
      z[static_cast<std::size_t>(i)] =
          math::NormInv(std::clamp(u, 1e-16, 1.0 - 1e-16));
    }
  } else {
    for (auto& x : z) x = gen.Next();
    if (method == VarianceReduction::kMomentMatching) {
      const Moments m = SampleMoments(z);
      const double sd = std::sqrt(m.variance);
      for (auto& x : z) x = (x - m.mean) / sd;
    }
  }
  return z;
}

}  // namespace

std::uint64_t MixSeed(std::uint64_t seed, std::uint64_t chunk) {
  std::uint64_t z = seed + 0x9E3779B97F4A7C15ULL * (chunk + 1);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

std::optional<McResult> MonteCarloEuropean(const VanillaOption& option,
                                           const TreeMarket& market,
                                           const McOptions& options) {
  if (option.exercise != Exercise::kEuropean || options.paths < 32) {
    return std::nullopt;
  }
  const double t = option.maturity;
  const double sigma = market.volatility;
  const Terminal terminal{
      market.spot, (market.rate - market.yield - 0.5 * sigma * sigma) * t,
      sigma * std::sqrt(t)};
  const double disc = std::exp(-market.rate * t);
  const int n = options.paths;

  switch (options.method) {
    case VarianceReduction::kNone:
    case VarianceReduction::kAntithetic:
    case VarianceReduction::kControlVariate:
    case VarianceReduction::kImportanceSampling: {
      random::NormalGenerator gen(options.seed);
      // Importance sampling: draw z ~ N(theta, 1) with theta centring S_T on
      // the strike when the option is out of the money, and weight by the
      // likelihood ratio exp(-theta z + theta^2 / 2).
      double theta = 0.0;
      if (options.method == VarianceReduction::kImportanceSampling) {
        const double to_strike =
            (std::log(option.strike / market.spot) - terminal.mu) /
            terminal.vol;
        theta = option.option_type == OptionType::kCall
                    ? std::max(to_strike, 0.0)
                    : std::min(to_strike, 0.0);
      }
      std::vector<double> f(static_cast<std::size_t>(n));
      std::vector<double> control(static_cast<std::size_t>(n));
      for (std::size_t i = 0; i < f.size(); ++i) {
        const double z = gen.Next();
        if (options.method == VarianceReduction::kAntithetic) {
          f[i] = 0.5 * (IntrinsicValue(option, terminal(z)) +
                        IntrinsicValue(option, terminal(-z)));
        } else if (options.method == VarianceReduction::kImportanceSampling) {
          const double y = z + theta;
          f[i] = IntrinsicValue(option, terminal(y)) *
                 std::exp(-theta * y + 0.5 * theta * theta);
        } else {
          const double s = terminal(z);
          f[i] = IntrinsicValue(option, s);
          control[i] = s;
        }
      }
      if (options.method == VarianceReduction::kControlVariate) {
        const double expected =
            market.spot * std::exp((market.rate - market.yield) * t);
        const Moments mf = SampleMoments(f);
        const Moments mc = SampleMoments(control);
        double cov = 0.0;
        for (std::size_t i = 0; i < f.size(); ++i) {
          cov += (f[i] - mf.mean) * (control[i] - mc.mean);
        }
        cov /= (n - 1.0);
        const double b = cov / mc.variance;
        for (std::size_t i = 0; i < f.size(); ++i) {
          f[i] -= b * (control[i] - expected);
        }
      }
      const Moments m = SampleMoments(f);
      return McResult{disc * m.mean, disc * std::sqrt(m.variance / n), n};
    }
    case VarianceReduction::kStratified:
    case VarianceReduction::kMomentMatching:
    case VarianceReduction::kQuasiRandom: {
      const int per_batch = n / kBatches;
      std::vector<double> batch_means;
      for (int b = 0; b < kBatches; ++b) {
        const auto z =
            BatchNormals(options.method, per_batch,
                         MixSeed(options.seed, static_cast<std::uint64_t>(b)));
        double sum = 0.0;
        for (double x : z) sum += IntrinsicValue(option, terminal(x));
        batch_means.push_back(sum / per_batch);
      }
      const Moments m = SampleMoments(batch_means);
      return McResult{disc * m.mean, disc * std::sqrt(m.variance / kBatches),
                      per_batch * kBatches};
    }
  }
  return std::nullopt;
}

std::optional<McResult> SimulateGbmPathsErased(
    const TreeMarket& market, double t, int steps,
    const std::function<double(std::span<const double>)>& payoff,
    const McOptions& options) {
  return SimulateGbmPaths(market, t, steps, payoff, options);
}

}  // namespace ofod::pricers
