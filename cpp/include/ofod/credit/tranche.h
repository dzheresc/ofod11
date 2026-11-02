// Copyright 2026 Denis Zhereshchin
//
// Tranche losses in a securitization waterfall (Hull, Ch 8).

#ifndef OFOD_CREDIT_TRANCHE_H_
#define OFOD_CREDIT_TRANCHE_H_

#include <span>
#include <vector>

namespace ofod::credit {

// A tranche absorbs pool losses between its attachment and detachment
// points, both fractions of the pool principal.
struct Tranche {
  double attachment = 0.0;
  double detachment = 1.0;
};

// Fraction of the tranche's principal lost for a pool loss fraction.
[[nodiscard]] double TrancheLoss(const Tranche& tranche, double pool_loss);

// Loss fraction of each tranche, in input order.
[[nodiscard]] std::vector<double> WaterfallLosses(
    std::span<const Tranche> tranches, double pool_loss);

}  // namespace ofod::credit

#endif  // OFOD_CREDIT_TRANCHE_H_
