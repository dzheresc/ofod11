// Copyright 2026 Denis Zhereshchin

#include "ofod/models/brownian.h"

#include <cmath>
#include <cstddef>

namespace ofod::models {

std::vector<double> ArithmeticBrownianMotion::Path(
    double x0, double dt, std::span<const double> shocks) const {
  std::vector<double> path{x0};
  path.reserve(shocks.size() + 1);
  const double root = std::sqrt(dt);
  for (double eps : shocks) {
    path.push_back(path.back() + drift * dt + volatility * root * eps);
  }
  return path;
}

std::vector<double> GeometricBrownianMotion::EulerPath(
    double s0, double dt, std::span<const double> shocks) const {
  std::vector<double> path{s0};
  path.reserve(shocks.size() + 1);
  const double root = std::sqrt(dt);
  for (double eps : shocks) {
    const double s = path.back();
    path.push_back(s + drift * s * dt + volatility * s * root * eps);
  }
  return path;
}

std::vector<double> GeometricBrownianMotion::ExactPath(
    double s0, double dt, std::span<const double> shocks) const {
  std::vector<double> path{s0};
  path.reserve(shocks.size() + 1);
  const double mu = (drift - 0.5 * volatility * volatility) * dt;
  const double jump = volatility * std::sqrt(dt);
  for (double eps : shocks) {
    path.push_back(path.back() * std::exp(mu + jump * eps));
  }
  return path;
}

double GeometricBrownianMotion::ExpectedValue(double s0, double t) const {
  return s0 * std::exp(drift * t);
}

double GeometricBrownianMotion::Variance(double s0, double t) const {
  return s0 * s0 * std::exp(2.0 * drift * t) *
         (std::exp(volatility * volatility * t) - 1.0);
}

double GeometricBrownianMotion::LogMean(double s0, double t) const {
  return std::log(s0) + (drift - 0.5 * volatility * volatility) * t;
}

double GeometricBrownianMotion::LogStdDev(double t) const {
  return volatility * std::sqrt(t);
}

Interval GeometricBrownianMotion::ConfidenceInterval(double s0, double t,
                                                     double z) const {
  const double m = LogMean(s0, t);
  const double s = LogStdDev(t);
  return {std::exp(m - z * s), std::exp(m + z * s)};
}

double QuadraticVariation(std::span<const double> path) {
  double sum = 0.0;
  for (std::size_t i = 1; i < path.size(); ++i) {
    const double dx = path[i] - path[i - 1];
    sum += dx * dx;
  }
  return sum;
}

}  // namespace ofod::models
