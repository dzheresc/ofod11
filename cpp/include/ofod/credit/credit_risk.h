// Copyright 2026 Denis Zhereshchin
//
// Credit risk (Hull, Ch 24): Merton's structural model, Gaussian copula
// default correlation and the Vasicek worst-case default rate.

#ifndef OFOD_CREDIT_CREDIT_RISK_H_
#define OFOD_CREDIT_CREDIT_RISK_H_

#include <optional>

namespace ofod::credit {

struct MertonResult {
  double asset_value;          // V0.
  double asset_volatility;     // sigma_V.
  double default_probability;  // N(-d2), risk-neutral.
  double debt_value;           // V0 - E0.
};

// Solves Merton's two equations (eqs. 24.3, 24.4) for V0 and sigma_V given
// equity E0 with volatility sigma_E and debt D due at T, by 2-D Newton.
// Returns std::nullopt for non-positive inputs or no convergence.
[[nodiscard]] std::optional<MertonResult> MertonSolve(double equity,
                                                      double equity_vol,
                                                      double debt, double rate,
                                                      double t);

// P(both default by T) for marginal default probabilities q1, q2 under a
// Gaussian copula with correlation rho (Ch 24.8).
[[nodiscard]] double JointDefaultProbability(double q1, double q2, double rho);

// Default probability conditional on the factor F (eq. 24.10):
// N((N^{-1}(Q) - sqrt(rho) F) / sqrt(1 - rho)).
[[nodiscard]] double ConditionalDefaultProbability(double q, double rho,
                                                   double factor);

// Worst-case default rate at confidence X (eq. 24.11).
[[nodiscard]] double Wcdr(double pd, double rho, double confidence);

// Vasicek credit VaR of a large homogeneous portfolio: exposure x LGD x
// WCDR (Ch 24.9, Hull's retail example).
[[nodiscard]] double VasicekCreditLoss(double exposure, double pd, double lgd,
                                       double rho, double confidence);

}  // namespace ofod::credit

#endif  // OFOD_CREDIT_CREDIT_RISK_H_
