// Copyright 2026 Denis Zhereshchin
//
// Wiener processes and geometric Brownian motion (Hull, Ch 14-15).
//
// Paths are built from explicit standard normal shocks, one per step, so
// that any generator (or the book's own epsilons) can drive them.

#ifndef OFOD_MODELS_BROWNIAN_H_
#define OFOD_MODELS_BROWNIAN_H_

#include <span>
#include <vector>

namespace ofod::models {

struct Interval {
  double lower;
  double upper;
};

// dx = a dt + b dz (generalized Wiener process, Ch 14.2).
struct ArithmeticBrownianMotion {
  double drift = 0.0;
  double volatility = 1.0;

  // x0 followed by one value per shock: x += a dt + b sqrt(dt) eps.
  [[nodiscard]] std::vector<double> Path(double x0, double dt,
                                         std::span<const double> shocks) const;
};

// dS = mu S dt + sigma S dz (Ch 14.3).
struct GeometricBrownianMotion {
  double drift = 0.0;  // mu (or r - q in a risk-neutral world).
  double volatility = 0.0;

  // Discretised as in Table 14.1: dS = mu S dt + sigma S sqrt(dt) eps.
  [[nodiscard]] std::vector<double> EulerPath(
      double s0, double dt, std::span<const double> shocks) const;
  // Exact: S *= exp((mu - sigma^2/2) dt + sigma sqrt(dt) eps) (eq. 21.15).
  [[nodiscard]] std::vector<double> ExactPath(
      double s0, double dt, std::span<const double> shocks) const;

  // Lognormal property (eqs. 15.3-15.5).
  [[nodiscard]] double ExpectedValue(double s0, double t) const;
  [[nodiscard]] double Variance(double s0, double t) const;
  [[nodiscard]] double LogMean(double s0, double t) const;
  [[nodiscard]] double LogStdDev(double t) const;
  // Range containing S_T with probability implied by `z` standard
  // deviations of ln S_T, e.g. z = 1.96 for 95%.
  [[nodiscard]] Interval ConfidenceInterval(double s0, double t,
                                            double z) const;
};

// Sum of squared increments; for a Wiener path it tends to T (Ch 14.2).
[[nodiscard]] double QuadraticVariation(std::span<const double> path);

}  // namespace ofod::models

#endif  // OFOD_MODELS_BROWNIAN_H_
