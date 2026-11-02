// Copyright 2026 Denis Zhereshchin
//
// Gauss-Hermite quadrature, used to integrate over the common factor of
// one-factor copula models (Hull, Ch 24-25).

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

}  // namespace ofod::math

#endif  // OFOD_MATH_QUADRATURE_H_
