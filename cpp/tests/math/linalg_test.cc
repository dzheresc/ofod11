// Copyright 2026 Denis Zhereshchin
//
// Jacobi eigen-decomposition and Nelder-Mead.

#include "ofod/math/linalg.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstddef>
#include <span>

#include "ofod/math/optimize.h"

namespace ofod::math {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("SymmetricEigen reconstructs the matrix", "[math][ch22]") {
  const Matrix a = {{4.0, 1.0, -2.0, 0.5},
                    {1.0, 3.0, 0.0, 1.5},
                    {-2.0, 0.0, 5.0, -1.0},
                    {0.5, 1.5, -1.0, 2.0}};
  const auto e = SymmetricEigen(a);
  REQUIRE(e.has_value());
  for (std::size_t k = 1; k < e->values.size(); ++k) {
    CHECK(e->values[k - 1] >= e->values[k]);
  }
  for (std::size_t i = 0; i < 4; ++i) {
    for (std::size_t j = 0; j < 4; ++j) {
      double sum = 0.0;
      double dot = 0.0;
      for (std::size_t k = 0; k < 4; ++k) {
        sum += e->vectors[k][i] * e->values[k] * e->vectors[k][j];
        dot += e->vectors[i][k] * e->vectors[j][k];
      }
      CHECK_THAT(sum, WithinAbs(a[i][j], 1e-12));
      CHECK_THAT(dot, WithinAbs(i == j ? 1.0 : 0.0, 1e-12));
    }
  }
}

TEST_CASE("SymmetricEigen rejects bad input", "[math][ch22]") {
  CHECK_FALSE(SymmetricEigen({}).has_value());
  CHECK_FALSE(SymmetricEigen({{1.0, 2.0}}).has_value());
  CHECK_FALSE(SymmetricEigen({{1.0, 2.0}, {0.0, 1.0}}).has_value());
  const auto diag = SymmetricEigen({{1.0, 0.0}, {0.0, 3.0}});
  REQUIRE(diag.has_value());
  CHECK(diag->values[0] == 3.0);
  CHECK(diag->vectors[0][1] == 1.0);
}

TEST_CASE("Nelder-Mead finds the Rosenbrock minimum", "[math][ch23]") {
  const Objective rosenbrock = [](std::span<const double> x) {
    return 100.0 * std::pow(x[1] - x[0] * x[0], 2) + std::pow(1.0 - x[0], 2);
  };
  constexpr std::array<double, 2> kStep = {0.5, 0.5};
  const Minimum m = NelderMead(rosenbrock, {-1.2, 1.0}, kStep);
  CHECK(m.converged);
  CHECK_THAT(m.x[0], WithinAbs(1.0, 1e-6));
  CHECK_THAT(m.x[1], WithinAbs(1.0, 1e-6));
  const Minimum capped =
      NelderMead(rosenbrock, {-1.2, 1.0}, kStep, {.max_iterations = 5});
  CHECK_FALSE(capped.converged);
  CHECK(capped.iterations == 5);
}

}  // namespace
}  // namespace ofod::math
