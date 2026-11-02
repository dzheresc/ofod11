// Copyright 2026 Denis Zhereshchin
//
// Finite difference methods for vanilla options (Hull, Ch 21.8).

#ifndef OFOD_PRICERS_FINITE_DIFFERENCE_H_
#define OFOD_PRICERS_FINITE_DIFFERENCE_H_

#include <optional>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"

namespace ofod::pricers {

enum class FdScheme {
  kExplicit,       // Hull's explicit method (eq. 21.28).
  kImplicit,       // Fully implicit (eq. 21.25); theta = 1.
  kCrankNicolson,  // theta = 1/2.
};

// How the implicit schemes impose early exercise.
enum class AmericanSolver {
  kPsor,        // Projected SOR: solves the linear complementarity problem.
  kProjection,  // Solve, then take the max with intrinsic value (Hull's
                // Table 21.5); first-order accurate in time.
};

// Uniform grid in S (Hull's layout) or, with log_space, in ln S. A log
// grid needs s_min > 0; an S grid starts at s_min (normally 0).
struct FdGrid {
  double s_min = 0.0;
  double s_max = 0.0;
  int price_steps = 100;  // Intervals in S (or ln S).
  int time_steps = 100;
  bool log_space = false;
};

// Rolls the option back from expiry. American options use `american` in the
// implicit schemes and the max with intrinsic value in the explicit one.
// The value at S0 is interpolated linearly between grid nodes. Returns
// std::nullopt for an invalid grid (too few steps, s_max <= s_min, s0
// outside it, or s_min <= 0 on a log grid).
[[nodiscard]] std::optional<double> PriceFiniteDifference(
    const instruments::VanillaOption& option, const TreeMarket& market,
    const FdGrid& grid, FdScheme scheme,
    AmericanSolver american = AmericanSolver::kPsor);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_FINITE_DIFFERENCE_H_
