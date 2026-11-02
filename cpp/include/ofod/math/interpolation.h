// Copyright 2026 Denis Zhereshchin
//
// Piecewise-linear interpolation with flat extrapolation.

#ifndef OFOD_MATH_INTERPOLATION_H_
#define OFOD_MATH_INTERPOLATION_H_

#include <optional>
#include <span>
#include <vector>

namespace ofod::math {

class LinearInterpolator {
 public:
  // Returns nullopt unless xs is non-empty, strictly increasing and the same
  // length as ys.
  static std::optional<LinearInterpolator> Create(std::vector<double> xs,
                                                  std::vector<double> ys);

  // Interpolates at `x`; flat beyond the first and last points.
  [[nodiscard]] double operator()(double x) const;

  std::span<const double> xs() const { return xs_; }
  std::span<const double> ys() const { return ys_; }

 private:
  LinearInterpolator(std::vector<double> xs, std::vector<double> ys)
      : xs_(std::move(xs)), ys_(std::move(ys)) {}

  std::vector<double> xs_;
  std::vector<double> ys_;
};

}  // namespace ofod::math

#endif  // OFOD_MATH_INTERPOLATION_H_
