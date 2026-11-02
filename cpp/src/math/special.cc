// Copyright 2026 Denis Zhereshchin

#include "ofod/math/special.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ofod::math {
namespace {

constexpr double kEps = 1e-16;
constexpr int kMaxIterations = 10'000;

double SeriesP(double a, double x) {
  double term = 1.0 / a;
  double sum = term;
  for (int n = 1; n < kMaxIterations; ++n) {
    term *= x / (a + n);
    sum += term;
    if (std::abs(term) < std::abs(sum) * kEps) break;
  }
  return sum * std::exp(-x + a * std::log(x) - std::lgamma(a));
}

double ContinuedFractionQ(double a, double x) {
  constexpr double kTiny = 1e-300;
  double b = x + 1.0 - a;
  double c = 1.0 / kTiny;
  double d = 1.0 / b;
  double h = d;
  for (int i = 1; i < kMaxIterations; ++i) {
    const double an = -i * (i - a);
    b += 2.0;
    d = an * d + b;
    if (std::abs(d) < kTiny) d = kTiny;
    c = b + an / c;
    if (std::abs(c) < kTiny) c = kTiny;
    d = 1.0 / d;
    const double delta = d * c;
    h *= delta;
    if (std::abs(delta - 1.0) < kEps) break;
  }
  return std::exp(-x + a * std::log(x) - std::lgamma(a)) * h;
}

}  // namespace

double RegularizedGammaP(double a, double x) {
  if (!(a > 0.0) || !(x >= 0.0)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if (x == 0.0) return 0.0;
  if (std::isinf(x)) return 1.0;
  if (x < a + 1.0) return SeriesP(a, x);
  return 1.0 - ContinuedFractionQ(a, x);
}

double NoncentralChiSquareCdf(double x, double dof, double lambda) {
  if (!(dof > 0.0) || !(lambda >= 0.0)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if (x <= 0.0) return 0.0;
  const double half = 0.5 * lambda;
  if (half == 0.0) return RegularizedGammaP(0.5 * dof, 0.5 * x);
  const double mode = std::floor(half);
  auto weight = [half](double j) {
    return std::exp(-half + j * std::log(half) - std::lgamma(j + 1.0));
  };
  auto term = [&](double j) {
    return weight(j) * RegularizedGammaP(0.5 * dof + j, 0.5 * x);
  };
  double sum = term(mode);
  for (double j = mode + 1.0; j < mode + kMaxIterations; j += 1.0) {
    const double t = term(j);
    sum += t;
    if (weight(j) < kEps * 1e-2) break;
  }
  for (double j = mode - 1.0; j >= 0.0; j -= 1.0) {
    sum += term(j);
    if (weight(j) < kEps * 1e-2) break;
  }
  return std::clamp(sum, 0.0, 1.0);
}

}  // namespace ofod::math
