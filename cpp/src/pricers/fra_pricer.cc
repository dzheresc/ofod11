// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/fra_pricer.h"

namespace ofod::pricers {

double SimpleForwardRate(const curves::ZeroCurve& curve, double t1, double t2) {
  return (curve.DiscountFactor(t1) / curve.DiscountFactor(t2) - 1.0) /
         (t2 - t1);
}

double FraValue(const instruments::Fra& fra, const curves::ZeroCurve& curve,
                instruments::Side side) {
  const double tau = fra.end - fra.start;
  const double forward = SimpleForwardRate(curve, fra.start, fra.end);
  const double value = fra.notional * (fra.fixed_rate - forward) * tau *
                       curve.DiscountFactor(fra.end);
  return instruments::Sign(side) * value;
}

}  // namespace ofod::pricers
