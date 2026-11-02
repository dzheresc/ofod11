// Copyright 2026 Denis Zhereshchin

#include "ofod/math/quadrature.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <vector>

namespace ofod::math {

std::optional<Quadrature> GaussHermite(int n) {
  if (n < 1 || n > 200) return std::nullopt;
  constexpr int kMaxNewton = 100;
  const double pim4 = 1.0 / std::pow(std::numbers::pi, 0.25);
  const auto size = static_cast<std::size_t>(n);
  std::vector<double> x(size);
  std::vector<double> w(size);
  const int m = (n + 1) / 2;
  double z = 0.0;
  for (int i = 0; i < m; ++i) {
    // Initial guesses (Numerical Recipes, gauher).
    if (i == 0) {
      z = std::sqrt(2.0 * n + 1.0) -
          1.85575 * std::pow(2.0 * n + 1.0, -1.0 / 6.0);
    } else if (i == 1) {
      z -= 1.14 * std::pow(n, 0.426) / z;
    } else if (i == 2) {
      z = 1.86 * z - 0.86 * x[0];
    } else if (i == 3) {
      z = 1.91 * z - 0.91 * x[1];
    } else {
      z = 2.0 * z - x[static_cast<std::size_t>(i - 2)];
    }
    double pp = 0.0;
    bool converged = false;
    for (int it = 0; it < kMaxNewton; ++it) {
      double p1 = pim4;
      double p2 = 0.0;
      for (int j = 1; j <= n; ++j) {
        const double p3 = p2;
        p2 = p1;
        p1 = z * std::sqrt(2.0 / j) * p2 - std::sqrt((j - 1.0) / j) * p3;
      }
      pp = std::sqrt(2.0 * n) * p2;
      const double z1 = z;
      z = z1 - p1 / pp;
      if (std::abs(z - z1) <= 3e-14) {
        converged = true;
        break;
      }
    }
    if (!converged) return std::nullopt;
    const auto lo = static_cast<std::size_t>(i);
    const auto hi = size - 1 - lo;
    x[lo] = z;
    x[hi] = -z;
    w[lo] = 2.0 / (pp * pp);
    w[hi] = w[lo];
  }
  return Quadrature{.nodes = std::move(x), .weights = std::move(w)};
}

std::optional<Quadrature> GaussHermiteNormal(int n) {
  auto q = GaussHermite(n);
  if (!q) return std::nullopt;
  for (double& x : q->nodes) x *= std::numbers::sqrt2;
  for (double& w : q->weights) w /= std::sqrt(std::numbers::pi);
  return q;
}

}  // namespace ofod::math
