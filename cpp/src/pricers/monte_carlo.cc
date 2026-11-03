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

// Vanilla payoff max(phi (S - K), 0); the same values as IntrinsicValue,
// inlined into the path loops.
struct Payoff {
  double phi;
  double strike;
  explicit Payoff(const VanillaOption& option)
      : phi(option.option_type == OptionType::kCall ? 1.0 : -1.0),
        strike(option.strike) {}
  double operator()(double s) const {
    return std::max(phi * (s - strike), 0.0);
  }
};

// Terminal price for a standard normal draw.
struct Terminal {
  double s0;
  double mu;   // (r - q - sigma^2/2) T.
  double vol;  // sigma sqrt(T).
  double operator()(double z) const { return s0 * std::exp(mu + vol * z); }
};

// Standard normals for one batch of `n` draws. `Gen` supplies the
// pseudo-random draws for kMomentMatching.
template <class Gen>
std::vector<double> BatchNormals(VarianceReduction method, int n,
                                 std::uint64_t seed) {
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
    Gen gen(seed);
    for (auto& x : z) x = gen.Next();
    if (method == VarianceReduction::kMomentMatching) {
      const Moments m = SampleMoments(z);
      const double sd = std::sqrt(m.variance);
      for (auto& x : z) x = (x - m.mean) / sd;
    }
  }
  return z;
}

// Plain, antithetic, control-variate and importance-sampling estimates:
// one independent draw per path.
template <class Gen>
McResult IndependentDraws(const VanillaOption& option, const TreeMarket& market,
                          const McOptions& options, const Terminal& terminal,
                          double disc) {
  const int n = options.paths;
  const Payoff payoff(option);
  Gen gen(options.seed);
  // Importance sampling: draw z ~ N(theta, 1) with theta centring S_T on
  // the strike when the option is out of the money, and weight by the
  // likelihood ratio exp(-theta z + theta^2 / 2).
  double theta = 0.0;
  if (options.method == VarianceReduction::kImportanceSampling) {
    const double to_strike =
        (std::log(option.strike / market.spot) - terminal.mu) / terminal.vol;
    theta = option.option_type == OptionType::kCall ? std::max(to_strike, 0.0)
                                                    : std::min(to_strike, 0.0);
  }
  const bool control_variate =
      options.method == VarianceReduction::kControlVariate;
  std::vector<double> f;
  std::vector<double> control;
  f.reserve(static_cast<std::size_t>(n));
  if (control_variate) control.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    const double z = gen.Next();
    if (options.method == VarianceReduction::kAntithetic) {
      f.push_back(0.5 * (payoff(terminal(z)) + payoff(terminal(-z))));
    } else if (options.method == VarianceReduction::kImportanceSampling) {
      const double y = z + theta;
      f.push_back(payoff(terminal(y)) *
                  std::exp(-theta * y + 0.5 * theta * theta));
    } else {
      const double s = terminal(z);
      f.push_back(payoff(s));
      if (control_variate) control.push_back(s);
    }
  }
  if (control_variate) {
    const double t = option.maturity;
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

// Stratified, moment-matched and quasi-random estimates: the standard
// error comes from kBatches independent batches.
template <class Gen>
McResult BatchedDraws(const VanillaOption& option, const McOptions& options,
                      const Terminal& terminal, double disc) {
  const int per_batch = options.paths / kBatches;
  const Payoff payoff(option);
  std::vector<double> batch_means;
  for (int b = 0; b < kBatches; ++b) {
    const auto z =
        BatchNormals<Gen>(options.method, per_batch,
                          MixSeed(options.seed, static_cast<std::uint64_t>(b)));
    double sum = 0.0;
    for (double x : z) sum += payoff(terminal(x));
    batch_means.push_back(sum / per_batch);
  }
  const Moments m = SampleMoments(batch_means);
  return McResult{disc * m.mean, disc * std::sqrt(m.variance / kBatches),
                  per_batch * kBatches};
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
  const bool ziggurat = options.rng == Rng::kZiggurat;
  switch (options.method) {
    case VarianceReduction::kNone:
    case VarianceReduction::kAntithetic:
    case VarianceReduction::kControlVariate:
    case VarianceReduction::kImportanceSampling:
      return ziggurat ? IndependentDraws<random::ZigguratNormalGenerator>(
                            option, market, options, terminal, disc)
                      : IndependentDraws<random::NormalGenerator>(
                            option, market, options, terminal, disc);
    case VarianceReduction::kStratified:
    case VarianceReduction::kMomentMatching:
    case VarianceReduction::kQuasiRandom:
      return ziggurat ? BatchedDraws<random::ZigguratNormalGenerator>(
                            option, options, terminal, disc)
                      : BatchedDraws<random::NormalGenerator>(option, options,
                                                              terminal, disc);
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
