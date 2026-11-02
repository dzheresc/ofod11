// Copyright 2026 Denis Zhereshchin

#include "ofod/credit/tranche.h"

#include <algorithm>

namespace ofod::credit {

double TrancheLoss(const Tranche& tranche, double pool_loss) {
  const double width = tranche.detachment - tranche.attachment;
  return std::clamp((pool_loss - tranche.attachment) / width, 0.0, 1.0);
}

std::vector<double> WaterfallLosses(std::span<const Tranche> tranches,
                                    double pool_loss) {
  std::vector<double> losses;
  losses.reserve(tranches.size());
  for (const auto& t : tranches) losses.push_back(TrancheLoss(t, pool_loss));
  return losses;
}

}  // namespace ofod::credit
