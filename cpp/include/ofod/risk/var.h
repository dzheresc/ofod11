// Copyright 2026 Denis Zhereshchin
//
// Value at risk and expected shortfall (Hull, Ch 22). Losses are positive
// numbers; VaR and ES are reported as positive losses.

#ifndef OFOD_RISK_VAR_H_
#define OFOD_RISK_VAR_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "ofod/math/linalg.h"

namespace ofod::risk {

// VaR of a normally distributed loss: mean + sd N^{-1}(X).
[[nodiscard]] double NormalVar(double sd, double confidence, double mean = 0.0);

// ES of a normal loss: mean + sd phi(N^{-1}(X)) / (1 - X).
[[nodiscard]] double NormalExpectedShortfall(double sd, double confidence,
                                             double mean = 0.0);

// T-day VaR = 1-day VaR * sqrt(T) (eq. 22.1).
[[nodiscard]] double ScaleToHorizon(double one_day, double days);

// Hull's historical simulation convention: with n scenarios the X VaR is
// the k-th worst loss, k = n (1 - X) rounded, and the ES is the mean of the
// k worst. Returns std::nullopt if k < 1.
[[nodiscard]] std::optional<double> HistoricalVar(
    std::span<const double> losses, double confidence);
[[nodiscard]] std::optional<double> HistoricalExpectedShortfall(
    std::span<const double> losses, double confidence);

// Volatility-scaled scenarios (Ch 22.3): changes[i] * current_vol / vols[i].
// Returns std::nullopt for a size mismatch or a non-positive volatility.
[[nodiscard]] std::optional<std::vector<double>> VolatilityAdjustedChanges(
    std::span<const double> changes, std::span<const double> vols,
    double current_vol);

// Standard deviation of sum_i alpha_i dx_i / x_i for exposures alpha_i,
// daily volatilities sigma_i and a correlation matrix (eq. 22.3). Returns
// std::nullopt for inconsistent sizes.
[[nodiscard]] std::optional<double> PortfolioStdDev(
    std::span<const double> exposures, std::span<const double> vols,
    const math::Matrix& correlation);

struct Moments {
  double mean;
  double std_dev;
  double skewness;
};

// Moments of dP = delta dx + gamma dx^2 / 2 for dx ~ N(0, sd^2) (Ch 22.6).
[[nodiscard]] Moments DeltaGammaMoments(double delta, double gamma,
                                        double sd_change);

// VaR from the Cornish-Fisher (1 - X) quantile of the P&L:
// w = z + (z^2 - 1) skew / 6, VaR = -(mean + w sd).
[[nodiscard]] double CornishFisherVar(const Moments& pnl, double confidence);

struct VarEstimate {
  double var;
  double expected_shortfall;
};

using PnlFunction = std::function<double(std::span<const double>)>;

// Monte Carlo VaR with full revaluation (Ch 22.6): draws factor changes
// from N(0, covariance), revalues with `pnl` and applies the historical
// convention to the simulated losses. Returns std::nullopt if the
// covariance is not positive definite or paths < 1 / (1 - X).
[[nodiscard]] std::optional<VarEstimate> MonteCarloVar(
    const PnlFunction& pnl, const math::Matrix& covariance, double confidence,
    int paths, std::uint64_t seed);

// Kupiec's likelihood ratio for `exceptions` VaR breaches in
// `observations` days when the breach probability is p (Ch 22.8); compare
// with chi-square(1), 3.84 at 5%.
[[nodiscard]] double KupiecStatistic(int observations, int exceptions,
                                     double p);

// P(X >= m) for X ~ Binomial(n, p).
[[nodiscard]] double BinomialTailProbability(int n, int m, double p);

struct PcaResult {
  std::vector<double> variances;  // Factor variances, descending.
  math::Matrix loadings;          // loadings[k] = factor k's loadings.
};

// Principal components of the sample covariance of `observations` (rows
// are dates, columns variables) (Ch 22.9). Returns std::nullopt for fewer
// than two rows or ragged rows.
[[nodiscard]] std::optional<PcaResult> PrincipalComponents(
    const math::Matrix& observations);

// Standard deviation of a portfolio with `exposures` to each variable when
// moves are driven by the given factors (loadings k x n, factor_sds k).
// Returns std::nullopt for inconsistent sizes.
[[nodiscard]] std::optional<double> PcaStdDev(
    std::span<const double> exposures, const math::Matrix& loadings,
    std::span<const double> factor_sds);

}  // namespace ofod::risk

#endif  // OFOD_RISK_VAR_H_
