// Copyright 2026 Denis Zhereshchin

#include "ofod/math/linalg.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <optional>
#include <vector>

namespace ofod::math {
namespace {

constexpr int kMaxSweeps = 100;

double OffDiagonalNorm(const Matrix& a) {
  double sum = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    for (std::size_t j = i + 1; j < a.size(); ++j) sum += a[i][j] * a[i][j];
  }
  return std::sqrt(sum);
}

}  // namespace

std::optional<Eigen> SymmetricEigen(const Matrix& input) {
  const std::size_t n = input.size();
  if (n == 0) return std::nullopt;
  double scale = 0.0;
  for (const auto& row : input) {
    if (row.size() != n) return std::nullopt;
    for (double x : row) scale = std::max(scale, std::abs(x));
  }
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i + 1; j < n; ++j) {
      if (std::abs(input[i][j] - input[j][i]) > 1e-12 * std::max(scale, 1.0)) {
        return std::nullopt;
      }
    }
  }
  Matrix a = input;
  Matrix v(n, std::vector<double>(n, 0.0));
  for (std::size_t i = 0; i < n; ++i) v[i][i] = 1.0;
  const double tol = 1e-15 * std::max(scale, 1e-300);
  int sweep = 0;
  for (; sweep < kMaxSweeps && OffDiagonalNorm(a) > tol; ++sweep) {
    for (std::size_t p = 0; p < n; ++p) {
      for (std::size_t q = p + 1; q < n; ++q) {
        if (a[p][q] == 0.0) continue;
        // Rotation angle that zeroes a[p][q] (Golub and Van Loan 8.5.2).
        const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
        const double t = std::copysign(1.0, theta) /
                         (std::abs(theta) + std::sqrt(theta * theta + 1.0));
        const double c = 1.0 / std::sqrt(t * t + 1.0);
        const double s = t * c;
        for (std::size_t k = 0; k < n; ++k) {
          const double akp = a[k][p];
          const double akq = a[k][q];
          a[k][p] = c * akp - s * akq;
          a[k][q] = s * akp + c * akq;
        }
        for (std::size_t k = 0; k < n; ++k) {
          const double apk = a[p][k];
          const double aqk = a[q][k];
          a[p][k] = c * apk - s * aqk;
          a[q][k] = s * apk + c * aqk;
        }
        for (std::size_t k = 0; k < n; ++k) {
          const double vkp = v[k][p];
          const double vkq = v[k][q];
          v[k][p] = c * vkp - s * vkq;
          v[k][q] = s * vkp + c * vkq;
        }
      }
    }
  }
  if (OffDiagonalNorm(a) > tol) return std::nullopt;

  std::vector<std::size_t> order(n);
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(
      order.begin(), order.end(),
      [&a](std::size_t i, std::size_t j) { return a[i][i] > a[j][j]; });
  Eigen out;
  for (std::size_t k : order) {
    out.values.push_back(a[k][k]);
    std::vector<double> column(n);
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      column[i] = v[i][k];
      sum += column[i];
    }
    if (sum < 0.0) {
      for (double& x : column) x = -x;
    }
    out.vectors.push_back(std::move(column));
  }
  return out;
}

}  // namespace ofod::math
