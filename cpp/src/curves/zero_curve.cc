// Copyright 2026 Denis Zhereshchin

#include "ofod/curves/zero_curve.h"

#include <cmath>
#include <cstdlib>
#include <utility>

namespace ofod::curves {

std::optional<ZeroCurve> ZeroCurve::Create(std::vector<double> times,
                                           std::vector<double> zero_rates) {
  auto interpolator =
      math::LinearInterpolator::Create(std::move(times), std::move(zero_rates));
  if (!interpolator) return std::nullopt;
  return ZeroCurve(*std::move(interpolator));
}

ZeroCurve ZeroCurve::Flat(double zero_rate) {
  auto curve = Create({1.0}, {zero_rate});
  if (!curve) std::abort();  // A single pillar is always valid.
  return *std::move(curve);
}

double ZeroCurve::DiscountFactor(double t) const {
  return std::exp(-ZeroRate(t) * t);
}

double ZeroCurve::ForwardRate(double t1, double t2) const {
  return (ZeroRate(t2) * t2 - ZeroRate(t1) * t1) / (t2 - t1);
}

}  // namespace ofod::curves
