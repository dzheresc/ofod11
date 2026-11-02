// Copyright 2026 Denis Zhereshchin
//
// Derivative-free minimisation (Nelder-Mead), used for maximum likelihood
// fits such as GARCH(1,1) in Ch 23.

#ifndef OFOD_MATH_OPTIMIZE_H_
#define OFOD_MATH_OPTIMIZE_H_

#include <functional>
#include <span>
#include <vector>

namespace ofod::math {

struct Minimum {
  std::vector<double> x;
  double value;
  int iterations;
  bool converged;
};

struct NelderMeadOptions {
  double f_tol = 1e-12;  // Spread of function values across the simplex.
  double x_tol = 1e-10;  // Largest vertex distance from the best vertex.
  int max_iterations = 10'000;
};

using Objective = std::function<double(std::span<const double>)>;

// Minimises `f` from `x0` with an initial simplex x0 + step_i e_i, using the
// standard coefficients (reflection 1, expansion 2, contraction 1/2, shrink
// 1/2). `step` must have the same size as `x0`.
[[nodiscard]] Minimum NelderMead(const Objective& f,
                                 const std::vector<double>& x0,
                                 std::span<const double> step,
                                 const NelderMeadOptions& options = {});

}  // namespace ofod::math

#endif  // OFOD_MATH_OPTIMIZE_H_
