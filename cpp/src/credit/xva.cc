// Copyright 2026 Denis Zhereshchin

#include "ofod/credit/xva.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ofod/curves/credit_curve.h"
#include "ofod/math/linalg.h"
#include "ofod/random/normal_generator.h"

namespace ofod::credit {
namespace {

double Quantile(std::vector<double> x, double q) {
  std::sort(x.begin(), x.end());
  const double pos = q * static_cast<double>(x.size() - 1);
  const auto lo = static_cast<std::size_t>(std::floor(pos));
  const std::size_t hi = std::min(lo + 1, x.size() - 1);
  return x[lo] + (pos - static_cast<double>(lo)) * (x[hi] - x[lo]);
}

}  // namespace

std::optional<ExposureProfile> ProfileFromPaths(std::span<const double> times,
                                                const math::Matrix& values,
                                                double pfe_quantile,
                                                double threshold) {
  if (values.size() < 2) return std::nullopt;
  const std::size_t n = times.size();
  for (const auto& path : values) {
    if (path.size() != n) return std::nullopt;
  }
  ExposureProfile profile;
  profile.times.assign(times.begin(), times.end());
  const auto paths = static_cast<double>(values.size());
  std::vector<double> column(values.size());
  for (std::size_t j = 0; j < n; ++j) {
    double ee = 0.0;
    double ene = 0.0;
    for (std::size_t p = 0; p < values.size(); ++p) {
      const double v = values[p][j];
      column[p] = std::min(std::max(v, 0.0), threshold);
      ee += column[p];
      ene += std::min(std::max(-v, 0.0), threshold);
    }
    profile.expected_exposure.push_back(ee / paths);
    profile.expected_negative_exposure.push_back(ene / paths);
    profile.potential_future_exposure.push_back(Quantile(column, pfe_quantile));
  }
  return profile;
}

std::optional<ExposureProfile> SimulateForwardExposure(
    double spot, double strike, double rate, double volatility, double maturity,
    int steps, int paths, std::uint64_t seed, double pfe_quantile,
    double threshold) {
  if (steps < 1 || paths < 2 || !(maturity > 0.0) || !(spot > 0.0)) {
    return std::nullopt;
  }
  const double dt = maturity / steps;
  const double drift = (rate - 0.5 * volatility * volatility) * dt;
  const double diffusion = volatility * std::sqrt(dt);
  std::vector<double> times;
  for (int j = 1; j <= steps; ++j) times.push_back(j * dt);
  random::NormalGenerator rng(seed);
  math::Matrix values(static_cast<std::size_t>(paths),
                      std::vector<double>(times.size()));
  for (auto& path : values) {
    double s = spot;
    for (std::size_t j = 0; j < times.size(); ++j) {
      s *= std::exp(drift + diffusion * rng.Next());
      const double t = times[j];
      const double value = s - strike * std::exp(-rate * (maturity - t));
      path[j] = value * std::exp(-rate * t);
    }
  }
  return ProfileFromPaths(times, values, pfe_quantile, threshold);
}

std::optional<double> Cva(std::span<const double> times,
                          std::span<const double> exposures,
                          const curves::CreditCurve& curve, double recovery) {
  if (times.size() != exposures.size()) return std::nullopt;
  double total = 0.0;
  double start = 0.0;
  for (std::size_t i = 0; i < times.size(); ++i) {
    total += curve.DefaultProbability(start, times[i]) * exposures[i];
    start = times[i];
  }
  return (1.0 - recovery) * total;
}

std::optional<double> Dva(std::span<const double> times,
                          std::span<const double> negative_exposures,
                          const curves::CreditCurve& own_curve,
                          double own_recovery) {
  return Cva(times, negative_exposures, own_curve, own_recovery);
}

std::optional<double> FundingValueAdjustment(std::span<const double> times,
                                             std::span<const double> exposures,
                                             double funding_spread) {
  if (times.size() != exposures.size()) return std::nullopt;
  double total = 0.0;
  double start = 0.0;
  for (std::size_t i = 0; i < times.size(); ++i) {
    total += funding_spread * (times[i] - start) * exposures[i];
    start = times[i];
  }
  return total;
}

}  // namespace ofod::credit
