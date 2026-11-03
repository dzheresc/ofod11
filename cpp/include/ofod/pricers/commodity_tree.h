// Copyright 2026 Denis Zhereshchin
//
// Hull's trinomial tree for a mean-reverting commodity price (Ch 35.4):
// x = ln S follows dx = (theta(t) - a x) dt + sigma dz, the tree is built
// in x as for Hull-White, and each step is shifted so that the expected
// price matches the futures price for that maturity.

#ifndef OFOD_PRICERS_COMMODITY_TREE_H_
#define OFOD_PRICERS_COMMODITY_TREE_H_

#include <optional>
#include <span>
#include <vector>

#include "ofod/instruments/option.h"
#include "ofod/pricers/short_rate_tree.h"

namespace ofod::pricers {

class CommodityTree {
 public:
  // `futures[i]` is the futures price for maturity (i + 1) dt, so the tree
  // has futures.size() steps. dx = sigma sqrt(3 dt) and jmax is the
  // smallest integer greater than 0.184 / (a dt). Returns nullopt for
  // non-positive prices, dt, a or sigma, or no futures.
  static std::optional<CommodityTree> Build(double spot,
                                            std::span<const double> futures,
                                            double a, double sigma, double dt);

  [[nodiscard]] int steps() const { return steps_; }
  [[nodiscard]] double dt() const { return dt_; }
  [[nodiscard]] double dx() const { return dx_; }
  [[nodiscard]] int jmax() const { return jmax_; }
  // alpha_i with ln S = alpha_i + j dx at step i; alpha_0 = ln spot.
  [[nodiscard]] std::span<const double> alphas() const { return alphas_; }

  [[nodiscard]] int Width(int step) const;
  [[nodiscard]] double Price(int step, int j) const;
  [[nodiscard]] const Branch& BranchAt(int j) const;
  // Risk-neutral probabilities of reaching each node of step i, indexed by
  // j + Width(i).
  [[nodiscard]] std::span<const double> Probabilities(int step) const;

  // Expectation one step back of values at step + 1, discounted at the
  // constant continuously compounded rate `rate`.
  [[nodiscard]] std::vector<double> Rollback(std::span<const double> next,
                                             int step, double rate) const;

 private:
  CommodityTree() = default;

  int steps_ = 0;
  double dt_ = 0.0;
  double dx_ = 0.0;
  int jmax_ = 0;
  std::vector<double> alphas_;
  // S = e^{alpha_i} e^{j dx}: both factors are tabulated so that Price()
  // needs no exp() (M12 profiling in docs/PERF.md).
  std::vector<double> step_level_;      // e^{alpha_i}, indexed by step.
  std::vector<double> node_growth_;     // e^{j dx}, indexed by j + jmax.
  std::vector<Branch> branches_;        // Indexed by j + jmax.
  std::vector<std::vector<double>> p_;  // Node probabilities per step.
};

// European or American option on the spot commodity price expiring at
// `expiry_step`. Returns nullopt if the step is outside the tree.
[[nodiscard]] std::optional<double> CommodityTreeOption(
    const CommodityTree& tree, instruments::OptionType type,
    instruments::Exercise exercise, double strike, int expiry_step,
    double rate);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_COMMODITY_TREE_H_
