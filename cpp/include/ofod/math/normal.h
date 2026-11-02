// Copyright 2026 Denis Zhereshchin
//
// Standard normal distribution helpers (Hull, Ch 15.9).

#ifndef OFOD_MATH_NORMAL_H_
#define OFOD_MATH_NORMAL_H_

namespace ofod::math {

// Returns the standard normal probability density at `x`.
[[nodiscard]] double NormPdf(double x) noexcept;

// Returns the standard normal cumulative distribution N(x), computed with
// std::erfc to full double precision.
[[nodiscard]] double NormCdf(double x) noexcept;

// Returns Hull's polynomial approximation to N(x) (Abramowitz-Stegun 26.2.17,
// absolute error below 7.5e-8).
[[nodiscard]] double NormCdfPoly(double x) noexcept;

// Returns the inverse of N: x with N(x) = p for p in (0, 1). Acklam's
// rational approximation refined by one Halley step (full double
// precision). Returns -inf / +inf at p = 0 / 1 and NaN outside [0, 1].
[[nodiscard]] double NormInv(double p) noexcept;

// Returns M(a, b; rho) = P(X <= a, Y <= b) for standard normals with
// correlation rho in [-1, 1] (Genz's BVND algorithm, ~1e-15 accuracy).
[[nodiscard]] double BivariateNormCdf(double a, double b, double rho) noexcept;

}  // namespace ofod::math

#endif  // OFOD_MATH_NORMAL_H_
