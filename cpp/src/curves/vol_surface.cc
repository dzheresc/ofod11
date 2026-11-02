// Copyright 2026 Denis Zhereshchin

#include "ofod/curves/vol_surface.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ofod::curves {
namespace {

bool StrictlyIncreasing(std::span<const double> xs) {
  return !xs.empty() && std::adjacent_find(xs.begin(), xs.end(),
                                           std::greater_equal<>()) == xs.end();
}

// Index i with xs[i] <= x < xs[i + 1]; assumes xs[0] < x < xs.back().
std::size_t Bracket(std::span<const double> xs, double x) {
  const auto it = std::upper_bound(xs.begin(), xs.end(), x);
  return static_cast<std::size_t>(std::distance(xs.begin(), it)) - 1;
}

}  // namespace

std::optional<VolSurface> VolSurface::Create(
    std::vector<double> maturities, std::vector<double> strikes,
    std::vector<std::vector<double>> volatilities,
    VolInterpolation interpolation) {
  if (!StrictlyIncreasing(maturities) || !StrictlyIncreasing(strikes) ||
      maturities.front() <= 0.0 || volatilities.size() != maturities.size()) {
    return std::nullopt;
  }
  for (const auto& row : volatilities) {
    if (row.size() != strikes.size()) return std::nullopt;
    if (std::ranges::any_of(row, [](double v) { return !(v > 0.0); })) {
      return std::nullopt;
    }
  }
  return VolSurface(std::move(maturities), std::move(strikes),
                    std::move(volatilities), interpolation);
}

double VolSurface::RowVolatility(std::size_t i, double strike) const {
  const auto& row = volatilities_[i];
  if (strike <= strikes_.front()) return row.front();
  if (strike >= strikes_.back()) return row.back();
  const std::size_t j = Bracket(strikes_, strike);
  const double w = (strike - strikes_[j]) / (strikes_[j + 1] - strikes_[j]);
  return row[j] + w * (row[j + 1] - row[j]);
}

double VolSurface::Volatility(double strike, double t) const {
  if (t <= maturities_.front()) return RowVolatility(0, strike);
  if (t >= maturities_.back()) {
    return RowVolatility(maturities_.size() - 1, strike);
  }
  const std::size_t i = Bracket(maturities_, t);
  const double t0 = maturities_[i];
  const double t1 = maturities_[i + 1];
  const double w = (t - t0) / (t1 - t0);
  const double v0 = RowVolatility(i, strike);
  const double v1 = RowVolatility(i + 1, strike);
  if (interpolation_ == VolInterpolation::kLinearVol) {
    return v0 + w * (v1 - v0);
  }
  const double var0 = v0 * v0 * t0;
  const double var1 = v1 * v1 * t1;
  return std::sqrt((var0 + w * (var1 - var0)) / t);
}

}  // namespace ofod::curves
