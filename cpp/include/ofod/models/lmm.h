// Copyright 2026 Denis Zhereshchin
//
// Forward-rate models (Hull, Ch 33): the discrete HJM model and the LIBOR
// market model (LMM) simulated under the rolling spot measure, with the
// caplet-volatility bootstrap and Rebonato's swaption volatility.

#ifndef OFOD_MODELS_LMM_H_
#define OFOD_MODELS_LMM_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "ofod/instruments/option.h"
#include "ofod/pricers/monte_carlo.h"

namespace ofod::models {

using Matrix = std::vector<std::vector<double>>;

// Forward rates F_k for [t_k, t_{k+1}], t_k = k accrual, k = 0, ..., n - 1,
// with stationary volatilities: loadings[i] are the factor volatilities of
// a forward rate whose reset is i + 1 accrual periods away (Lambda_i, Ch
// 33.2), so Lambda_i^2 = sum_q loadings[i][q]^2.
struct LmmSpec {
  double accrual;
  std::vector<double> forwards;
  Matrix loadings;  // n - 1 rows, one column per factor.
};

// Lambda_i from caplet volatilities sigma_k for resets t_k = k accrual,
// k = 1, ..., n - 1, using sigma_k^2 t_k = sum_{i=1}^{k} Lambda_{k-i}^2
// accrual. Returns nullopt if the implied variance would be negative.
[[nodiscard]] std::optional<std::vector<double>> LmmVolatilitiesFromCaplets(
    std::span<const double> caplet_vols);

// Single-factor loadings from Lambda_i.
[[nodiscard]] Matrix OneFactorLoadings(std::span<const double> lambdas);

// Today's discount factors P(0, t_k), k = 0, ..., n, implied by the
// forwards.
[[nodiscard]] std::vector<double> LmmDiscountFactors(const LmmSpec& spec);

// One simulated path: Forward(j, k) = F_k(t_j) for j <= k, and
// Numeraire(j) = prod_{i < j} (1 + accrual F_i(t_i)), the rolled-over
// bank account at t_j.
class LmmPath {
 public:
  explicit LmmPath(int n);
  [[nodiscard]] int size() const { return n_; }
  [[nodiscard]] double Forward(int j, int k) const {
    return forwards_[Offset(j) + static_cast<std::size_t>(k - j)];
  }
  [[nodiscard]] double Numeraire(int j) const {
    return numeraire_[static_cast<std::size_t>(j)];
  }
  // The resets F_k(t_k).
  [[nodiscard]] double Reset(int k) const { return Forward(k, k); }

  double& MutableForward(int j, int k) {
    return forwards_[Offset(j) + static_cast<std::size_t>(k - j)];
  }
  double& MutableNumeraire(int j) {
    return numeraire_[static_cast<std::size_t>(j)];
  }

 private:
  [[nodiscard]] std::size_t Offset(int j) const;

  int n_;
  std::vector<double> forwards_;   // Rows j = 0..n-1 of n - j values.
  std::vector<double> numeraire_;  // j = 0..n.
};

// Payoff in units of the numeraire: sum_j cash_j / Numeraire(j).
using LmmPayoff = std::function<double(const LmmPath&)>;

// Monte Carlo under the spot measure with Hull's log-Euler step (Ch 33.2)
// (Hull): ln F_k += [sum_{i=j+1}^{k} tau F_i lambda_{i-j-1} .
// lambda_{k-j-1} / (1 + tau F_i) - |lambda_{k-j-1}|^2 / 2] tau +
// lambda_{k-j-1} . eps sqrt(tau). With `predictor_corrector` the drift is
// the average of the drifts at the start and at an Euler prediction of
// the end. The price is today's value, i.e. the mean payoff (B(0) = 1).
// Returns nullopt for an inconsistent spec.
struct LmmOptions {
  int paths = 10'000;
  std::uint64_t seed = 1;
  bool predictor_corrector = false;
};
[[nodiscard]] std::optional<pricers::McResult> LmmMonteCarlo(
    const LmmSpec& spec, const LmmPayoff& payoff, const LmmOptions& options);

// Black volatility for a European swaption expiring at t_start on the
// swap paying at t_{start+1}, ..., t_end, by Rebonato's frozen-weight
// approximation (Ch 33.2): s0^2 sigma^2 t_start = sum_{k,l} w_k w_l F_k
// F_l C_kl, with w_k = tau P(0, t_{k+1}) / A and C_kl the integrated
// covariance of ln F_k and ln F_l up to t_start.
[[nodiscard]] std::optional<double> RebonatoSwaptionVolatility(
    const LmmSpec& spec, int start, int end);

// Discrete HJM (Ch 33.1): instantaneous forwards on the grid T_i = i dt,
// i = 0, ..., m - 1, with one-factor volatility v(t, T). The drift of each
// forward is the discrete no-arbitrage drift, so zero-coupon bonds
// discounted at the simulated short rate are martingales.
struct HjmSpec {
  double dt;
  std::vector<double> forwards;  // f(0, T_i).
  std::function<double(double t, double maturity)> volatility;
};

// Monte Carlo price of a European option expiring at step `expiry` on a
// zero-coupon bond maturing at step `maturity` (strike 0 and a call gives
// the bond itself). Returns nullopt for steps outside the grid.
[[nodiscard]] std::optional<pricers::McResult> HjmZeroBondOption(
    const HjmSpec& spec, instruments::OptionType type, int expiry, int maturity,
    double strike, int paths, std::uint64_t seed);

}  // namespace ofod::models

#endif  // OFOD_MODELS_LMM_H_
