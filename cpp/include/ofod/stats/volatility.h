// Copyright 2026 Denis Zhereshchin
//
// Volatility and correlation models (Hull, Ch 23): EWMA, GARCH(1,1) with a
// maximum likelihood fit, the Ljung-Box test and a positive semi-definite
// check for correlation matrices. Returns are daily and variances per day.

#ifndef OFOD_STATS_VOLATILITY_H_
#define OFOD_STATS_VOLATILITY_H_

#include <optional>
#include <span>
#include <vector>

#include "ofod/math/linalg.h"

namespace ofod::stats {

// sigma_n^2 = lambda sigma_{n-1}^2 + (1 - lambda) u_{n-1}^2 (eq. 23.7).
[[nodiscard]] double EwmaVarianceUpdate(double variance, double ret,
                                        double lambda);

// cov_n = lambda cov_{n-1} + (1 - lambda) x_{n-1} y_{n-1} (eq. 23.13).
[[nodiscard]] double EwmaCovarianceUpdate(double covariance, double ret_x,
                                          double ret_y, double lambda);

// Variance estimates v[0..n]: v[0] = initial, v[i] uses returns[0..i-1].
[[nodiscard]] std::vector<double> EwmaVariances(std::span<const double> returns,
                                                double lambda, double initial);

struct Garch11 {
  double omega = 0.0;
  double alpha = 0.0;
  double beta = 0.0;

  // V_L = omega / (1 - alpha - beta) (eq. 23.10).
  [[nodiscard]] double LongRunVariance() const;

  // omega + alpha u^2 + beta sigma^2 (eq. 23.9).
  [[nodiscard]] double Update(double variance, double ret) const;

  // E[sigma_{n+t}^2] = V_L + (alpha + beta)^t (sigma_n^2 - V_L) (eq. 23.14).
  [[nodiscard]] double ExpectedVariance(double variance, double days) const;

  // Annualised volatility for an option lasting `days` (eq. 23.15).
  [[nodiscard]] double TermStructureVolatility(
      double variance, double days, double days_per_year = 252.0) const;

  // Hull's convention: v[1] = u[0]^2, v[i] = Update(v[i-1], u[i-1]); v[0]
  // is undefined and set to u[0]^2. Size equals returns.size().
  [[nodiscard]] std::vector<double> Variances(
      std::span<const double> returns) const;

  // sum_{i >= 1} (-ln v_i - u_i^2 / v_i) (Table 23.1).
  [[nodiscard]] double LogLikelihood(std::span<const double> returns) const;
};

// Maximum likelihood GARCH(1,1) by Nelder-Mead on (omega / s^2, alpha,
// beta), s^2 the sample variance. Returns std::nullopt for fewer than 10
// returns, a zero sample variance or a fit that does not converge.
[[nodiscard]] std::optional<Garch11> FitGarch11(
    std::span<const double> returns);

// EWMA log likelihood with v[1] = u[0]^2 (Hull's lambda estimation).
[[nodiscard]] double EwmaLogLikelihood(std::span<const double> returns,
                                       double lambda);

// Lag-k sample autocorrelation; std::nullopt unless 0 < k < n.
[[nodiscard]] std::optional<double> Autocorrelation(std::span<const double> x,
                                                    int lag);

// Q = n (n + 2) sum_{k=1}^{lags} rho_k^2 / (n - k) (Ch 23.5); std::nullopt
// unless 0 < lags < n.
[[nodiscard]] std::optional<double> LjungBox(std::span<const double> x,
                                             int lags);

// True if a symmetric matrix has no eigenvalue below -tolerance (Ch 23.8).
[[nodiscard]] bool IsPositiveSemidefinite(const math::Matrix& a,
                                          double tolerance = 1e-12);

}  // namespace ofod::stats

#endif  // OFOD_STATS_VOLATILITY_H_
