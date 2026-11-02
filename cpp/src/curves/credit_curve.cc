// Copyright 2026 Denis Zhereshchin

#include "ofod/curves/credit_curve.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace ofod::curves {

std::optional<CreditCurve> CreditCurve::Create(std::vector<double> times,
                                               std::vector<double> hazards) {
  if (times.empty() || times.size() != hazards.size() || times[0] <= 0.0) {
    return std::nullopt;
  }
  for (std::size_t i = 0; i < times.size(); ++i) {
    if (hazards[i] < 0.0 || (i > 0 && times[i] <= times[i - 1])) {
      return std::nullopt;
    }
  }
  return CreditCurve(std::move(times), std::move(hazards));
}

CreditCurve CreditCurve::Flat(double hazard) {
  return CreditCurve({1.0}, {hazard});
}

double CreditCurve::SurvivalProbability(double t) const {
  if (t <= 0.0) return 1.0;
  double integral = 0.0;
  double start = 0.0;
  for (std::size_t i = 0; i < times_.size(); ++i) {
    const double end = i + 1 == times_.size() ? t : std::min(t, times_[i]);
    if (end > start) integral += hazards_[i] * (end - start);
    if (t <= times_[i]) break;
    start = times_[i];
  }
  return std::exp(-integral);
}

double CreditCurve::Hazard(double t) const {
  for (std::size_t i = 0; i < times_.size(); ++i) {
    if (t <= times_[i]) return hazards_[i];
  }
  return hazards_.back();
}

double HazardFromSpread(double spread, double recovery) {
  return spread / (1.0 - recovery);
}

double AverageHazard(double cumulative_default, double t) {
  return -std::log(1.0 - cumulative_default) / t;
}

}  // namespace ofod::curves
