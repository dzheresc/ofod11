// Copyright 2026 Denis Zhereshchin
//
// Trinomial trees for one-factor short-rate models (Hull, Ch 32.6-32.8):
// the two-stage Hull-White construction for normal rates and the same tree
// in ln r for Black-Karasinski, both fitted exactly to today's zero curve,
// and the bond options and Bermudan swaptions priced on them.

#ifndef OFOD_PRICERS_SHORT_RATE_TREE_H_
#define OFOD_PRICERS_SHORT_RATE_TREE_H_

#include <optional>
#include <span>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/option.h"

namespace ofod::pricers {

using instruments::Exercise;
using instruments::OptionType;

// The tree is built in x with dx = a(t) - a x dt + sigma dz; the short
// rate (the dt-period rate) at node (i, j) is alpha_i + j dx for
// Hull-White and exp(alpha_i + j dx) for Black-Karasinski.
enum class ShortRateModel { kHullWhite, kBlackKarasinski };

struct Branch {
  int middle;  // j of the middle node reached at the next step.
  double up;
  double mid;
  double down;
};

// Hull's trinomial branching from node j with M = -a dt: normal branching
// inside the tree, branching (b) at j = -jmax and (c) at j = jmax.
[[nodiscard]] Branch HullBranch(int j, int jmax, double m);

class ShortRateTree {
 public:
  // Builds `steps` steps of length `dt`. jmax is the smallest integer
  // greater than 0.184 / (a dt), with M = -a dt as in Hull; a = 0 gives an
  // untruncated tree. Returns nullopt for non-positive dt, sigma or steps,
  // or if a Black-Karasinski alpha cannot be fitted.
  static std::optional<ShortRateTree> Build(
      const curves::ZeroCurve& curve, double a, double sigma, double dt,
      int steps, ShortRateModel model = ShortRateModel::kHullWhite);

  [[nodiscard]] int steps() const { return steps_; }
  [[nodiscard]] double dt() const { return dt_; }
  [[nodiscard]] double dx() const { return dx_; }
  [[nodiscard]] int jmax() const { return jmax_; }
  [[nodiscard]] std::span<const double> alphas() const { return alphas_; }

  // Highest |j| at step i: min(i, jmax).
  [[nodiscard]] int Width(int step) const;

  // dt-period short rate at node (step, j), |j| <= Width(step).
  [[nodiscard]] double Rate(int step, int j) const;

  // One-step discount factor exp(-r dt) at node (step, j).
  [[nodiscard]] double Discount(int step, int j) const;

  // Branching probabilities from node j (any step).
  [[nodiscard]] const Branch& BranchAt(int j) const;

  // Arrow-Debreu prices Q_{i, j} at step i, indexed by j + Width(i).
  [[nodiscard]] std::span<const double> ArrowDebreu(int step) const;

  // Discounted expectation one step back: values at step + 1 (indexed by
  // j + Width(step + 1)) to values at `step`.
  [[nodiscard]] std::vector<double> Rollback(std::span<const double> next,
                                             int step) const;

  // Price at node (step, j) of a unit zero-coupon bond maturing at
  // `maturity_step`, for every j at `step`.
  [[nodiscard]] std::vector<double> ZeroBondValues(int step,
                                                   int maturity_step) const;

 private:
  ShortRateTree() = default;

  int steps_ = 0;
  double dt_ = 0.0;
  double dx_ = 0.0;
  int jmax_ = 0;
  ShortRateModel model_ = ShortRateModel::kHullWhite;
  std::vector<double> alphas_;
  // Hull-White discount factors factor as e^{-alpha_i dt} e^{-j dx dt}, so
  // rollbacks need no exp() (Ch 32 profiling in docs/PERF.md).
  std::vector<double> step_discount_;   // Indexed by step.
  std::vector<double> node_discount_;   // Indexed by j + jmax.
  std::vector<Branch> branches_;        // Indexed by j + jmax.
  std::vector<std::vector<double>> q_;  // Arrow-Debreu prices per step.
};

// Cash flows on tree steps for bonds and swaps.
struct TreeCashFlow {
  int step;
  double amount;
};

// Option on a bond whose cash flows fall on tree steps, exercisable at
// `expiry_step` (European) or at any step up to it (American). The
// exercise value uses the bond's value excluding a coupon paid at the
// exercise step. Returns nullopt if a step is outside the tree.
[[nodiscard]] std::optional<double> TreeBondOption(
    const ShortRateTree& tree, OptionType type, Exercise exercise,
    int expiry_step, std::span<const TreeCashFlow> bond, double strike);

// Payer (kCall) or receiver (kPut) swaption on a swap ending at
// `end_step`, paying fixed `strike` every `period_steps` steps on a unit
// notional. The holder may enter, at any step in `exercise_steps`, the
// swap from that step to the end (a co-terminal Bermudan; one step gives
// a European). Each exercise step must lie on the payment grid
// (end_step - k period_steps). Returns nullopt for a bad schedule.
[[nodiscard]] std::optional<double> TreeSwaption(
    const ShortRateTree& tree, OptionType type, double strike, int period_steps,
    int end_step, std::span<const int> exercise_steps);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_SHORT_RATE_TREE_H_
