// Copyright 2026 Denis Zhereshchin

#include "ofod/math/optimize.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace ofod::math {
namespace {

using Point = std::vector<double>;

Point Affine(const Point& a, const Point& b, double t) {
  // a + t (b - a).
  Point out(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) out[i] = a[i] + t * (b[i] - a[i]);
  return out;
}

}  // namespace

Minimum NelderMead(const Objective& f, const std::vector<double>& x0,
                   std::span<const double> step,
                   const NelderMeadOptions& options) {
  const std::size_t n = x0.size();
  std::vector<Point> simplex(n + 1, x0);
  for (std::size_t i = 0; i < n; ++i) simplex[i + 1][i] += step[i];
  std::vector<double> values(n + 1);
  for (std::size_t i = 0; i <= n; ++i) values[i] = f(simplex[i]);

  std::vector<std::size_t> order(n + 1);
  int iteration = 0;
  bool converged = false;
  for (; iteration < options.max_iterations; ++iteration) {
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&values](std::size_t i, std::size_t j) {
                       return values[i] < values[j];
                     });
    std::vector<Point> sorted_points;
    std::vector<double> sorted_values;
    for (std::size_t i : order) {
      sorted_points.push_back(std::move(simplex[i]));
      sorted_values.push_back(values[i]);
    }
    simplex = std::move(sorted_points);
    values = std::move(sorted_values);

    double f_spread = 0.0;
    double x_spread = 0.0;
    for (std::size_t i = 1; i <= n; ++i) {
      f_spread = std::max(f_spread, std::abs(values[i] - values[0]));
      for (std::size_t j = 0; j < n; ++j) {
        x_spread = std::max(x_spread, std::abs(simplex[i][j] - simplex[0][j]));
      }
    }
    if (f_spread <= options.f_tol && x_spread <= options.x_tol) {
      converged = true;
      break;
    }

    Point centroid(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        centroid[j] += simplex[i][j] / static_cast<double>(n);
      }
    }
    const Point& worst = simplex[n];
    const Point reflected = Affine(centroid, worst, -1.0);
    const double f_reflected = f(reflected);
    if (f_reflected < values[0]) {
      const Point expanded = Affine(centroid, worst, -2.0);
      const double f_expanded = f(expanded);
      if (f_expanded < f_reflected) {
        simplex[n] = expanded;
        values[n] = f_expanded;
      } else {
        simplex[n] = reflected;
        values[n] = f_reflected;
      }
      continue;
    }
    if (f_reflected < values[n - 1]) {
      simplex[n] = reflected;
      values[n] = f_reflected;
      continue;
    }
    const bool outside = f_reflected < values[n];
    const Point contracted = outside ? Affine(centroid, reflected, 0.5)
                                     : Affine(centroid, worst, 0.5);
    const double f_contracted = f(contracted);
    if (f_contracted < (outside ? f_reflected : values[n])) {
      simplex[n] = contracted;
      values[n] = f_contracted;
      continue;
    }
    for (std::size_t i = 1; i <= n; ++i) {
      simplex[i] = Affine(simplex[0], simplex[i], 0.5);
      values[i] = f(simplex[i]);
    }
  }
  const auto best = static_cast<std::size_t>(std::distance(
      values.begin(), std::min_element(values.begin(), values.end())));
  return {.x = simplex[best],
          .value = values[best],
          .iterations = iteration,
          .converged = converged};
}

}  // namespace ofod::math
