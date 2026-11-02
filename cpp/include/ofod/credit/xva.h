// Copyright 2026 Denis Zhereshchin
//
// Credit value adjustments (Hull, Ch 9, with Monte Carlo exposure from Ch
// 24.6): exposure profiles with collateral thresholds, CVA, DVA and a
// funding value adjustment.

#ifndef OFOD_CREDIT_XVA_H_
#define OFOD_CREDIT_XVA_H_

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ofod/curves/credit_curve.h"
#include "ofod/math/linalg.h"

namespace ofod::credit {

struct ExposureProfile {
  std::vector<double> times;
  std::vector<double> expected_exposure;           // E[min(max(V, 0), H)].
  std::vector<double> expected_negative_exposure;  // E[min(max(-V, 0), H)].
  std::vector<double> potential_future_exposure;   // Quantile of the above.
};

// Profile from simulated (already discounted) values `values[path][j]` at
// `times[j]`. Collateral covers exposure above `threshold` H in both
// directions, leaving min(max(V, 0), H). Returns std::nullopt for ragged paths
// or fewer than 2 paths.
[[nodiscard]] std::optional<ExposureProfile> ProfileFromPaths(
    std::span<const double> times, const math::Matrix& values,
    double pfe_quantile = 0.975, double threshold = 1e300);

// Discounted exposure profile of a long forward (S_t - K e^{-r(T - t)})
// on a GBM asset under the risk-neutral measure, at `steps` equally spaced
// dates up to T.
[[nodiscard]] std::optional<ExposureProfile> SimulateForwardExposure(
    double spot, double strike, double rate, double volatility, double maturity,
    int steps, int paths, std::uint64_t seed, double pfe_quantile = 0.975,
    double threshold = 1e300);

// CVA = (1 - R) sum_i q_i v_i with q_i the counterparty's default
// probability on (t_{i-1}, t_i] and v_i the discounted expected exposure
// at that interval's midpoint (eq. 9.1). `times` are interval ends.
// Returns std::nullopt for mismatched sizes.
[[nodiscard]] std::optional<double> Cva(std::span<const double> times,
                                        std::span<const double> exposures,
                                        const curves::CreditCurve& curve,
                                        double recovery);

// DVA is the same formula on the bank's own curve and negative exposure.
[[nodiscard]] std::optional<double> Dva(
    std::span<const double> times, std::span<const double> negative_exposures,
    const curves::CreditCurve& own_curve, double own_recovery);

// FVA = sum_i s (t_i - t_{i-1}) v_i for a funding spread s on the
// discounted (net) exposure v_i (Ch 9.4).
[[nodiscard]] std::optional<double> FundingValueAdjustment(
    std::span<const double> times, std::span<const double> exposures,
    double funding_spread);

}  // namespace ofod::credit

#endif  // OFOD_CREDIT_XVA_H_
