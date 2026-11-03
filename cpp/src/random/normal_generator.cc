// Copyright 2026 Denis Zhereshchin

#include "ofod/random/normal_generator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ofod::random {

std::vector<double> NormalGenerator::Sample(int n) {
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(n > 0 ? n : 0));
  for (int i = 0; i < n; ++i) out.push_back(Next());
  return out;
}

std::optional<Matrix> Cholesky(const Matrix& a) {
  const std::size_t n = a.size();
  for (const auto& row : a) {
    if (row.size() != n) return std::nullopt;
  }
  constexpr double kSymmetryTolerance = 1e-12;
  Matrix l(n, std::vector<double>(n, 0.0));
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j <= i; ++j) {
      if (std::abs(a[i][j] - a[j][i]) > kSymmetryTolerance) return std::nullopt;
      double sum = a[i][j];
      for (std::size_t k = 0; k < j; ++k) sum -= l[i][k] * l[j][k];
      if (i == j) {
        if (sum <= 0.0) return std::nullopt;
        l[i][i] = std::sqrt(sum);
      } else {
        l[i][j] = sum / l[j][j];
      }
    }
  }
  return l;
}

std::vector<double> Correlate(const Matrix& lower, std::span<const double> z) {
  std::vector<double> out(lower.size(), 0.0);
  for (std::size_t i = 0; i < lower.size(); ++i) {
    // A local accumulator keeps the sum in a register; accumulating into
    // out[i] made the compiler store and reload it on every term. The
    // summation order is unchanged, so results are bit-for-bit the same.
    const std::vector<double>& row = lower[i];
    const std::size_t terms = std::min(i + 1, z.size());
    double sum = 0.0;
    for (std::size_t k = 0; k < terms; ++k) sum += row[k] * z[k];
    out[i] = sum;
  }
  return out;
}

}  // namespace ofod::random
