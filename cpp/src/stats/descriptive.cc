// Copyright 2026 Denis Zhereshchin

#include "ofod/stats/descriptive.h"

#include <cmath>
#include <cstddef>
#include <numeric>

namespace ofod::stats {
namespace {

// Unchecked helpers; callers validate sizes first.
double MeanOf(std::span<const double> x) {
  return std::accumulate(x.begin(), x.end(), 0.0) /
         static_cast<double>(x.size());
}

double CovarianceOf(std::span<const double> x, std::span<const double> y) {
  const double mx = MeanOf(x);
  const double my = MeanOf(y);
  double sum = 0.0;
  for (std::size_t i = 0; i < x.size(); ++i) {
    sum += (x[i] - mx) * (y[i] - my);
  }
  return sum / static_cast<double>(x.size() - 1);
}

bool ValidPair(std::span<const double> x, std::span<const double> y) {
  return x.size() == y.size() && x.size() >= 2;
}

}  // namespace

std::optional<double> Mean(std::span<const double> x) {
  if (x.empty()) return std::nullopt;
  return MeanOf(x);
}

std::optional<double> Covariance(std::span<const double> x,
                                 std::span<const double> y) {
  if (!ValidPair(x, y)) return std::nullopt;
  return CovarianceOf(x, y);
}

std::optional<double> Variance(std::span<const double> x) {
  return Covariance(x, x);
}

std::optional<double> StdDev(std::span<const double> x) {
  if (!ValidPair(x, x)) return std::nullopt;
  return std::sqrt(CovarianceOf(x, x));
}

std::optional<double> Correlation(std::span<const double> x,
                                  std::span<const double> y) {
  if (!ValidPair(x, y)) return std::nullopt;
  const double sx = std::sqrt(CovarianceOf(x, x));
  const double sy = std::sqrt(CovarianceOf(y, y));
  if (sx == 0.0 || sy == 0.0) return std::nullopt;
  return CovarianceOf(x, y) / (sx * sy);
}

std::optional<Regression> LinearRegression(std::span<const double> x,
                                           std::span<const double> y) {
  if (!ValidPair(x, y)) return std::nullopt;
  const double var_x = CovarianceOf(x, x);
  if (var_x == 0.0) return std::nullopt;
  const double cov = CovarianceOf(x, y);
  const double slope = cov / var_x;
  const double intercept = MeanOf(y) - slope * MeanOf(x);
  const double var_y = CovarianceOf(y, y);
  const double r_squared = var_y == 0.0 ? 1.0 : slope * cov / var_y;
  return Regression{intercept, slope, r_squared};
}

}  // namespace ofod::stats
