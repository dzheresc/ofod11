// Copyright 2026 Denis Zhereshchin
//
// Small dense linear algebra: the symmetric eigenproblem (cyclic Jacobi),
// enough for PCA on a handful of rates and for semi-definiteness checks.

#ifndef OFOD_MATH_LINALG_H_
#define OFOD_MATH_LINALG_H_

#include <optional>
#include <vector>

namespace ofod::math {

using Matrix = std::vector<std::vector<double>>;

struct Eigen {
  std::vector<double> values;  // Descending.
  Matrix vectors;              // vectors[k] is the unit eigenvector of
                               // values[k], signed so its entries sum >= 0.
};

// Eigen-decomposition of a symmetric matrix by cyclic Jacobi rotations.
// Returns std::nullopt if `a` is empty, not square or not symmetric (to
// 1e-12 relative), or the sweeps do not converge.
[[nodiscard]] std::optional<Eigen> SymmetricEigen(const Matrix& a);

// Solves A x = b by Gaussian elimination with partial pivoting. Returns
// std::nullopt unless A is square, matches b and is non-singular.
[[nodiscard]] std::optional<std::vector<double>> SolveLinear(
    Matrix a, std::vector<double> b);

}  // namespace ofod::math

#endif  // OFOD_MATH_LINALG_H_
