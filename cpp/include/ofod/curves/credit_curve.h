// Copyright 2026 Denis Zhereshchin
//
// Default probability term structure with piecewise-constant hazard rates
// (Hull, Ch 24.1-24.3).

#ifndef OFOD_CURVES_CREDIT_CURVE_H_
#define OFOD_CURVES_CREDIT_CURVE_H_

#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ofod::curves {

class CreditCurve {
 public:
  // hazards[i] applies on (times[i-1], times[i]] with times[-1] = 0, and the
  // last hazard beyond times.back(). Returns std::nullopt unless `times` is
  // non-empty, positive and strictly increasing, the sizes match and no
  // hazard is negative.
  static std::optional<CreditCurve> Create(std::vector<double> times,
                                           std::vector<double> hazards);

  // The same hazard rate at every horizon.
  static CreditCurve Flat(double hazard);

  // Q(t) survival probability exp(-int_0^t lambda).
  [[nodiscard]] double SurvivalProbability(double t) const;

  // 1 - Q(t).
  [[nodiscard]] double DefaultProbability(double t) const {
    return 1.0 - SurvivalProbability(t);
  }

  // Probability of default in (t1, t2].
  [[nodiscard]] double DefaultProbability(double t1, double t2) const {
    return SurvivalProbability(t1) - SurvivalProbability(t2);
  }

  // Instantaneous hazard rate at t.
  [[nodiscard]] double Hazard(double t) const;

  std::span<const double> times() const { return times_; }
  std::span<const double> hazards() const { return hazards_; }

 private:
  CreditCurve(std::vector<double> times, std::vector<double> hazards)
      : times_(std::move(times)), hazards_(std::move(hazards)) {}

  std::vector<double> times_;
  std::vector<double> hazards_;
};

// lambda = s / (1 - R) from a credit spread (eq. 24.2).
[[nodiscard]] double HazardFromSpread(double spread, double recovery);

// Average hazard rate -ln(1 - Q(t)) / t from a cumulative default
// probability (eq. 24.1).
[[nodiscard]] double AverageHazard(double cumulative_default, double t);

}  // namespace ofod::curves

#endif  // OFOD_CURVES_CREDIT_CURVE_H_
