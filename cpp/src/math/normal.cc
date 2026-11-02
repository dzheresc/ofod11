// Copyright 2026 Denis Zhereshchin

#include "ofod/math/normal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>

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

namespace {

// Gauss-Legendre nodes (negative half) and weights for 6, 12, 20 points.
constexpr std::array<double, 3> kX6 = {-0.9324695142031522, -0.6612093864662647,
                                       -0.2386191860831970};
constexpr std::array<double, 3> kW6 = {0.1713244923791705, 0.3607615730481384,
                                       0.4679139345726904};
constexpr std::array<double, 6> kX12 = {
    -0.9815606342467191, -0.9041172563704750, -0.7699026741943050,
    -0.5873179542866171, -0.3678314989981802, -0.1252334085114692};
constexpr std::array<double, 6> kW12 = {
    0.04717533638651177, 0.1069393259953183, 0.1600783285433464,
    0.2031674267230659,  0.2334925365383547, 0.2491470458134029};
constexpr std::array<double, 10> kX20 = {
    -0.9931285991850949, -0.9639719272779138, -0.9122344282513259,
    -0.8391169718222188, -0.7463319064601508, -0.6360536807265150,
    -0.5108670019508271, -0.3737060887154196, -0.2277858511416451,
    -0.07652652113349733};
constexpr std::array<double, 10> kW20 = {
    0.01761400713915212, 0.04060142980038694, 0.06267204833410906,
    0.08327674157670475, 0.1019301198172404,  0.1181945319615184,
    0.1316886384491766,  0.1420961093183821,  0.1491729864726037,
    0.1527533871307259};

// P(X > h, Y > k) with correlation r (Genz 2004, "bvnu").
double UpperBivariate(double h, double k, double r) {
  std::span<const double> x;
  std::span<const double> w;
  if (std::abs(r) < 0.3) {
    x = kX6;
    w = kW6;
  } else if (std::abs(r) < 0.75) {
    x = kX12;
    w = kW12;
  } else {
    x = kX20;
    w = kW20;
  }
  constexpr double kTwoPi = 2.0 * std::numbers::pi;
  double hk = h * k;
  double bvn = 0.0;
  if (std::abs(r) < 0.925) {
    const double hs = (h * h + k * k) / 2.0;
    const double asr = std::asin(r);
    for (std::size_t i = 0; i < x.size(); ++i) {
      for (double sign : {-1.0, 1.0}) {
        const double sn = std::sin(asr * (1.0 + sign * x[i]) / 2.0);
        bvn += w[i] * std::exp((sn * hk - hs) / (1.0 - sn * sn));
      }
    }
    return bvn * asr / (2.0 * kTwoPi) + NormCdf(-h) * NormCdf(-k);
  }
  if (r < 0.0) {
    k = -k;
    hk = -hk;
  }
  if (std::abs(r) < 1.0) {
    const double as = (1.0 - r) * (1.0 + r);
    double a = std::sqrt(as);
    const double bs = (h - k) * (h - k);
    const double c = (4.0 - hk) / 8.0;
    const double d = (12.0 - hk) / 16.0;
    double asr = -(bs / as + hk) / 2.0;
    if (asr > -100.0) {
      bvn = a * std::exp(asr) *
            (1.0 - c * (bs - as) * (1.0 - d * bs / 5.0) / 3.0 +
             c * d * as * as / 5.0);
    }
    if (hk > -100.0) {
      const double b = std::sqrt(bs);
      const double sp = std::sqrt(kTwoPi) * NormCdf(-b / a);
      bvn -= std::exp(-hk / 2.0) * sp * b *
             (1.0 - c * bs * (1.0 - d * bs / 5.0) / 3.0);
    }
    a /= 2.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
      for (double sign : {-1.0, 1.0}) {
        const double xs = (a + sign * a * x[i]) * (a + sign * a * x[i]);
        const double rs = std::sqrt(1.0 - xs);
        asr = -(bs / xs + hk) / 2.0;
        if (asr > -100.0) {
          const double sp = 1.0 + c * xs * (1.0 + d * xs);
          const double ep =
              std::exp(-hk * xs / (2.0 * (1.0 + rs) * (1.0 + rs))) / rs;
          bvn += a * w[i] * std::exp(asr) * (ep - sp);
        }
      }
    }
    bvn = -bvn / kTwoPi;
  }
  if (r > 0.0) return bvn + NormCdf(-std::max(h, k));
  if (h >= k) return -bvn;
  const double l =
      h < 0.0 ? NormCdf(k) - NormCdf(h) : NormCdf(-h) - NormCdf(-k);
  return l - bvn;
}

}  // namespace

double BivariateNormCdf(double a, double b, double rho) noexcept {
  return std::clamp(UpperBivariate(-a, -b, rho), 0.0, 1.0);
}

}  // namespace ofod::math
