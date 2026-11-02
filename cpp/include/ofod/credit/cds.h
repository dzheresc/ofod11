// Copyright 2026 Denis Zhereshchin
//
// Credit default swaps (Hull, Ch 25.2-25.5): Hull's discrete valuation
// with defaults at the middle of each payment period, par spreads, hazard
// rate bootstrapping, fixed coupons with upfront payments, and CDS options.

#ifndef OFOD_CREDIT_CDS_H_
#define OFOD_CREDIT_CDS_H_

#include <optional>
#include <span>

#include "ofod/curves/credit_curve.h"
#include "ofod/curves/zero_curve.h"

namespace ofod::credit {

struct CdsTerms {
  double maturity = 5.0;
  int frequency = 4;  // Premium payments per year.
  double recovery = 0.4;
};

// Present values per unit notional.
struct CdsLegs {
  double premium_annuity;  // PV of paying 1 a year: regular payments plus
                           // accrual to a mid-period default (Table 25.2-3).
  double protection;       // PV of (1 - R) paid at mid-period defaults.
};

// Returns std::nullopt for a non-positive maturity or frequency.
[[nodiscard]] std::optional<CdsLegs> CdsLegValues(
    const CdsTerms& terms, const curves::CreditCurve& credit,
    const curves::ZeroCurve& discount);

// Spread that sets the two legs equal.
[[nodiscard]] std::optional<double> CdsParSpread(
    const CdsTerms& terms, const curves::CreditCurve& credit,
    const curves::ZeroCurve& discount);

// Value to the protection buyer paying `spread`: protection - spread x
// annuity (Ch 25.3).
[[nodiscard]] std::optional<double> CdsBuyerValue(
    const CdsTerms& terms, double spread, const curves::CreditCurve& credit,
    const curves::ZeroCurve& discount);

// Upfront paid by the buyer of a CDS with a fixed `coupon` (Ch 25.4).
[[nodiscard]] std::optional<double> CdsUpfront(
    const CdsTerms& terms, double coupon, const curves::CreditCurve& credit,
    const curves::ZeroCurve& discount);

// Piecewise-constant hazard rates that reprice par spreads quoted for
// increasing maturities (all other terms shared). Returns std::nullopt for
// mismatched or non-increasing inputs or an unreachable spread.
[[nodiscard]] std::optional<curves::CreditCurve> BootstrapCreditCurve(
    std::span<const double> maturities, std::span<const double> spreads,
    int frequency, double recovery, const curves::ZeroCurve& discount);

// Black's model for an option to buy (payer) or sell protection at
// `strike` on a forward CDS (Ch 25.6): annuity x Black(F, K, sigma, T).
[[nodiscard]] double CdsOptionPrice(bool payer, double forward_spread,
                                    double strike, double volatility,
                                    double expiry, double forward_annuity);

}  // namespace ofod::credit

#endif  // OFOD_CREDIT_CDS_H_
