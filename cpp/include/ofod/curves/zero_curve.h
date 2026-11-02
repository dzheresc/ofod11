// Copyright 2026 Denis Zhereshchin
//
// Zero-coupon yield curve (Hull, Ch 4.5 and 4.8).

#ifndef OFOD_CURVES_ZERO_CURVE_H_
#define OFOD_CURVES_ZERO_CURVE_H_

#include <optional>
#include <span>
#include <vector>

#include "ofod/math/interpolation.h"

namespace ofod::curves {

// Continuously compounded zero rates at pillar times (years), linearly
// interpolated in the zero rate and flat outside the pillars, as in Hull.
class ZeroCurve {
 public:
  // Returns nullopt unless `times` is non-empty, strictly increasing and the
  // same length as `zero_rates`.
  static std::optional<ZeroCurve> Create(std::vector<double> times,
                                         std::vector<double> zero_rates);

  // A curve with the same continuously compounded rate at all maturities.
  static ZeroCurve Flat(double zero_rate);

  // Continuously compounded zero rate for maturity `t`.
  [[nodiscard]] double ZeroRate(double t) const { return interpolator_(t); }

  // Present value of 1 paid at time `t`.
  [[nodiscard]] double DiscountFactor(double t) const;

  // Continuously compounded forward rate between `t1` and `t2` (t1 < t2).
  [[nodiscard]] double ForwardRate(double t1, double t2) const;

  std::span<const double> times() const { return interpolator_.xs(); }
  std::span<const double> zero_rates() const { return interpolator_.ys(); }

 private:
  explicit ZeroCurve(math::LinearInterpolator interpolator)
      : interpolator_(std::move(interpolator)) {}

  math::LinearInterpolator interpolator_;
};

}  // namespace ofod::curves

#endif  // OFOD_CURVES_ZERO_CURVE_H_
