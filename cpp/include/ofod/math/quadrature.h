// Copyright 2026 Denis Zhereshchin
//
// Gaussian quadrature: Gauss-Hermite for the common factor of one-factor
// copula models (Hull, Ch 24-25) and Gauss-Legendre for Fourier pricing
// integrals (Ch 27).

#ifndef OFOD_MATH_QUADRATURE_H_
#define OFOD_MATH_QUADRATURE_H_

#include <optional>
#include <vector>

namespace ofod::math {

struct Quadrature {
  std::vector<double> nodes;
  std::vector<double> weights;
};

// Nodes and weights with sum_i w_i f(x_i) ~ int e^{-x^2} f(x) dx, nodes in
// descending order (Newton iteration on orthonormal Hermite polynomials).
// Returns std::nullopt for n < 1 or n > 200.
[[nodiscard]] std::optional<Quadrature> GaussHermite(int n);

// The same rule rescaled for E[f(Z)], Z ~ N(0, 1): nodes x sqrt(2),
// weights / sqrt(pi).
[[nodiscard]] std::optional<Quadrature> GaussHermiteNormal(int n);

// Nodes (ascending) and weights for int_a^b f(x) dx with n points (Newton
// iteration on Legendre polynomials). Returns std::nullopt for n < 1,
// n > 200 or b <= a.
[[nodiscard]] std::optional<Quadrature> GaussLegendre(int n, double a,
                                                      double b);

}  // namespace ofod::math

#endif  // OFOD_MATH_QUADRATURE_H_
