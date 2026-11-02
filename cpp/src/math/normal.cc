// Copyright 2026 Denis Zhereshchin

#include "ofod/math/normal.h"

#include <array>
#include <cmath>
#include <limits>
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

double NormInv(double p) noexcept {
  constexpr std::array<double, 6> kA = {
      -3.969683028665376e+01, 2.209460984245205e+02,  -2.759285104469687e+02,
      1.383577518672690e+02,  -3.066479806614716e+01, 2.506628277459239e+00};
  constexpr std::array<double, 5> kB = {
      -5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
      6.680131188771972e+01, -1.328068155288572e+01};
  constexpr std::array<double, 6> kC = {
      -7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
      -2.549732539343734e+00, 4.374664141464968e+00,  2.938163982698783e+00};
  constexpr std::array<double, 4> kD = {
      7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
      3.754408661907416e+00};
  constexpr double kLow = 0.02425;
  if (std::isnan(p) || p < 0.0 || p > 1.0) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if (p == 0.0) return -std::numeric_limits<double>::infinity();
  if (p == 1.0) return std::numeric_limits<double>::infinity();
  double x;
  if (p < kLow || p > 1.0 - kLow) {
    const double q = std::sqrt(-2.0 * std::log(p < kLow ? p : 1.0 - p));
    x = (((((kC[0] * q + kC[1]) * q + kC[2]) * q + kC[3]) * q + kC[4]) * q +
         kC[5]) /
        ((((kD[0] * q + kD[1]) * q + kD[2]) * q + kD[3]) * q + 1.0);
    if (p > 1.0 - kLow) x = -x;
  } else {
    const double q = p - 0.5;
    const double r = q * q;
    x = (((((kA[0] * r + kA[1]) * r + kA[2]) * r + kA[3]) * r + kA[4]) * r +
         kA[5]) *
        q /
        (((((kB[0] * r + kB[1]) * r + kB[2]) * r + kB[3]) * r + kB[4]) * r +
         1.0);
  }
  // One Halley step on N(x) - p.
  const double e = NormCdf(x) - p;
  const double u = e / NormPdf(x);
  return x - u / (1.0 + 0.5 * x * u);
}

}  // namespace ofod::math
