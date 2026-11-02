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

}  // namespace ofod::math

#endif  // OFOD_MATH_NORMAL_H_
