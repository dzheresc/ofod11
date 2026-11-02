// Copyright 2026 Denis Zhereshchin
//
// One-dimensional root finders.

#ifndef OFOD_MATH_ROOTS_H_
#define OFOD_MATH_ROOTS_H_

#include <cmath>
#include <concepts>
#include <optional>
#include <utility>

namespace ofod::math {

struct RootOptions {
  double tolerance = 1e-12;
  int max_iterations = 200;
};

// Finds a root of `f` in [lo, hi] with Brent's method. Returns nullopt when
// f(lo) and f(hi) have the same sign or the iteration limit is reached.
template <std::invocable<double> F>
[[nodiscard]] std::optional<double> Brent(F&& f, double lo, double hi,
                                          RootOptions options = {}) {
  double a = lo;
  double b = hi;
  double fa = f(a);
  double fb = f(b);
  if (fa == 0.0) return a;
  if (fb == 0.0) return b;
  if ((fa > 0.0) == (fb > 0.0)) return std::nullopt;
  if (std::abs(fa) < std::abs(fb)) {
    std::swap(a, b);
    std::swap(fa, fb);
  }
  double c = a;
  double fc = fa;
  double d = b - a;
  bool bisected = true;
  for (int i = 0; i < options.max_iterations; ++i) {
    if (fb == 0.0 || std::abs(b - a) < options.tolerance) return b;
    double s;
    if (fa != fc && fb != fc) {
      // Inverse quadratic interpolation.
      s = a * fb * fc / ((fa - fb) * (fa - fc)) +
          b * fa * fc / ((fb - fa) * (fb - fc)) +
          c * fa * fb / ((fc - fa) * (fc - fb));
    } else {
      s = b - fb * (b - a) / (fb - fa);  // Secant.
    }
    const double mid = (3.0 * a + b) / 4.0;
    const bool outside = (s < std::min(mid, b)) || (s > std::max(mid, b));
    const bool slow = bisected ? std::abs(s - b) >= std::abs(b - c) / 2.0
                               : std::abs(s - b) >= std::abs(c - d) / 2.0;
    const bool tiny = bisected ? std::abs(b - c) < options.tolerance
                               : std::abs(c - d) < options.tolerance;
    if (outside || slow || tiny) {
      s = (a + b) / 2.0;
      bisected = true;
    } else {
      bisected = false;
    }
    const double fs = f(s);
    d = c;
    c = b;
    fc = fb;
    if ((fa > 0.0) != (fs > 0.0)) {
      b = s;
      fb = fs;
    } else {
      a = s;
      fa = fs;
    }
    if (std::abs(fa) < std::abs(fb)) {
      std::swap(a, b);
      std::swap(fa, fb);
    }
  }
  return std::nullopt;
}

// Finds a root of `f` by Newton-Raphson from `x0`, using derivative `df`.
// Returns nullopt on a zero derivative or when the iteration limit is hit.
template <std::invocable<double> F, std::invocable<double> DF>
[[nodiscard]] std::optional<double> Newton(F&& f, DF&& df, double x0,
                                           RootOptions options = {}) {
  double x = x0;
  for (int i = 0; i < options.max_iterations; ++i) {
    const double slope = df(x);
    if (slope == 0.0) return std::nullopt;
    const double step = f(x) / slope;
    x -= step;
    if (std::abs(step) < options.tolerance) return x;
  }
  return std::nullopt;
}

}  // namespace ofod::math

#endif  // OFOD_MATH_ROOTS_H_
