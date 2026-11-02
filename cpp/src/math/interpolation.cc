// Copyright 2026 Denis Zhereshchin

#include "ofod/math/interpolation.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace ofod::math {

std::optional<LinearInterpolator> LinearInterpolator::Create(
    std::vector<double> xs, std::vector<double> ys) {
  if (xs.empty() || xs.size() != ys.size()) return std::nullopt;
  if (std::adjacent_find(xs.begin(), xs.end(), std::greater_equal<>()) !=
      xs.end()) {
    return std::nullopt;
  }
  return LinearInterpolator(std::move(xs), std::move(ys));
}

double LinearInterpolator::operator()(double x) const {
  if (x <= xs_.front()) return ys_.front();
  if (x >= xs_.back()) return ys_.back();
  const auto upper = std::upper_bound(xs_.begin(), xs_.end(), x);
  const auto i = static_cast<std::size_t>(std::distance(xs_.begin(), upper));
  const double w = (x - xs_[i - 1]) / (xs_[i] - xs_[i - 1]);
  return ys_[i - 1] + w * (ys_[i] - ys_[i - 1]);
}

}  // namespace ofod::math
