// Copyright 2026 Denis Zhereshchin
//
// One-factor Gaussian copula pricing of synthetic CDO tranches (Hull, Ch
// 25.10-25.11): Gauss-Hermite integration over the factor and the
// Andersen-Sidenius-Basu recursion for the conditional loss distribution,
// implied (compound) correlation, and nth-to-default baskets by Monte Carlo.

#ifndef OFOD_CREDIT_COPULA_H_
#define OFOD_CREDIT_COPULA_H_

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ofod/credit/tranche.h"
#include "ofod/curves/zero_curve.h"

namespace ofod::credit {

// Distribution of the number of defaults among names with conditional
// default probabilities `p` (the recursion of Andersen et al.):
// result[k] = P(k defaults).
[[nodiscard]] std::vector<double> DefaultCountDistribution(
    std::span<const double> p);

struct CdoPool {
  int names = 125;
  double hazard = 0.01;  // Flat, the same for every name.
  double recovery = 0.4;
};

struct CdoTerms {
  double maturity = 5.0;
  int frequency = 4;
  int quadrature_points = 64;
};

struct TrancheLegs {
  double premium_annuity;  // PV of 1 a year on the outstanding tranche
                           // notional, as a fraction of tranche width.
  double protection;       // PV of tranche losses, same units.
};

// Expected loss of `tranche` by time t as a fraction of tranche width.
[[nodiscard]] std::optional<double> ExpectedTrancheLoss(
    const Tranche& tranche, const CdoPool& pool, double correlation, double t,
    int quadrature_points = 64);

// Returns std::nullopt for an invalid pool, tranche or terms.
[[nodiscard]] std::optional<TrancheLegs> TrancheLegValues(
    const Tranche& tranche, const CdoPool& pool, double correlation,
    const CdoTerms& terms, const curves::ZeroCurve& discount);

// Breakeven spread on the outstanding tranche notional.
[[nodiscard]] std::optional<double> TrancheSpread(
    const Tranche& tranche, const CdoPool& pool, double correlation,
    const CdoTerms& terms, const curves::ZeroCurve& discount);

// Correlation in (0, 1) that reproduces `spread` (Hull's implied or
// compound correlation, Ch 25.11): the lowest root on [1e-4, 0.999],
// found by scanning 40 sub-intervals and then Brent. Mezzanine tranches
// can have two roots or none. Returns std::nullopt if none is found.
[[nodiscard]] std::optional<double> ImpliedTrancheCorrelation(
    const Tranche& tranche, const CdoPool& pool, double spread,
    const CdoTerms& terms, const curves::ZeroCurve& discount);

struct BasketTerms {
  int names = 10;
  int nth = 1;
  double hazard = 0.01;
  double recovery = 0.4;
  double correlation = 0.3;
  double maturity = 5.0;
  int frequency = 4;
};

struct BasketResult {
  double spread;
  double std_error;  // Of the protection leg PV, relative to the spread.
};

// nth-to-default spread by Monte Carlo on a one-factor Gaussian copula
// with a flat discount rate (Ch 25.9). Returns std::nullopt for invalid
// terms or paths < 2.
[[nodiscard]] std::optional<BasketResult> NthToDefaultSpread(
    const BasketTerms& terms, double rate, int paths, std::uint64_t seed);

}  // namespace ofod::credit

#endif  // OFOD_CREDIT_COPULA_H_
