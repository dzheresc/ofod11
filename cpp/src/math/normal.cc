// Copyright 2026 Denis Zhereshchin

#include "ofod/math/normal.h"

#include <cmath>
#include <numbers>

namespace ofod::math {

double NormPdf(double x) noexcept {
  return std::exp(-0.5 * x * x) * std::numbers::inv_sqrtpi /
         std::numbers::sqrt2;
}

double NormCdf(double x) noexcept {
  return 0.5 * std::erfc(-x / std::numbers::sqrt2);
}

double NormCdfPoly(double x) noexcept {
  constexpr double kGamma = 0.2316419;
  constexpr double kA1 = 0.319381530;
  constexpr double kA2 = -0.356563782;
  constexpr double kA3 = 1.781477937;
  constexpr double kA4 = -1.821255978;
  constexpr double kA5 = 1.330274429;

  if (x < 0.0) return 1.0 - NormCdfPoly(-x);
  const double k = 1.0 / (1.0 + kGamma * x);
  const double poly = k * (kA1 + k * (kA2 + k * (kA3 + k * (kA4 + k * kA5))));
  return 1.0 - NormPdf(x) * poly;
}

}  // namespace ofod::math
