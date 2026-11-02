// Copyright 2026 Denis Zhereshchin
//
// Special functions: the regularized incomplete gamma function and the
// noncentral chi-square distribution used by the CEV model (Hull, Ch 27.1).

#ifndef OFOD_MATH_SPECIAL_H_
#define OFOD_MATH_SPECIAL_H_

namespace ofod::math {

// P(a, x) = gamma(a, x) / Gamma(a) for a > 0, x >= 0 (series below a + 1,
// Lentz continued fraction above). Returns NaN for invalid arguments.
[[nodiscard]] double RegularizedGammaP(double a, double x);

// P(X <= x) for X noncentral chi-square with `dof` > 0 degrees of freedom
// and noncentrality `lambda` >= 0: a Poisson(lambda/2) mixture of central
// chi-square CDFs, summed outwards from the largest Poisson weight.
[[nodiscard]] double NoncentralChiSquareCdf(double x, double dof,
                                            double lambda);

}  // namespace ofod::math

#endif  // OFOD_MATH_SPECIAL_H_
