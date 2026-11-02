// Copyright 2026 Denis Zhereshchin

#include "ofod/credit/cds.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "ofod/curves/credit_curve.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/math/normal.h"
#include "ofod/math/roots.h"

namespace ofod::credit {

std::optional<CdsLegs> CdsLegValues(const CdsTerms& terms,
                                    const curves::CreditCurve& credit,
                                    const curves::ZeroCurve& discount) {
  if (!(terms.maturity > 0.0) || terms.frequency < 1) return std::nullopt;
  const double dt = 1.0 / terms.frequency;
  const auto periods =
      static_cast<int>(std::ceil(terms.maturity * terms.frequency - 1e-9));
  double annuity = 0.0;
  double protection = 0.0;
  double start = 0.0;
  for (int i = 1; i <= periods; ++i) {
    const double end = std::min(i * dt, terms.maturity);
    const double length = end - start;
    const double mid = 0.5 * (start + end);
    const double default_prob = credit.DefaultProbability(start, end);
    const double df_mid = discount.DiscountFactor(mid);
    annuity += length * credit.SurvivalProbability(end) *
                   discount.DiscountFactor(end) +
               0.5 * length * default_prob * df_mid;
    protection += (1.0 - terms.recovery) * default_prob * df_mid;
    start = end;
  }
  return CdsLegs{.premium_annuity = annuity, .protection = protection};
}

std::optional<double> CdsParSpread(const CdsTerms& terms,
                                   const curves::CreditCurve& credit,
                                   const curves::ZeroCurve& discount) {
  const auto legs = CdsLegValues(terms, credit, discount);
  if (!legs) return std::nullopt;
  return legs->protection / legs->premium_annuity;
}

std::optional<double> CdsBuyerValue(const CdsTerms& terms, double spread,
                                    const curves::CreditCurve& credit,
                                    const curves::ZeroCurve& discount) {
  const auto legs = CdsLegValues(terms, credit, discount);
  if (!legs) return std::nullopt;
  return legs->protection - spread * legs->premium_annuity;
}

std::optional<double> CdsUpfront(const CdsTerms& terms, double coupon,
                                 const curves::CreditCurve& credit,
                                 const curves::ZeroCurve& discount) {
  return CdsBuyerValue(terms, coupon, credit, discount);
}

std::optional<curves::CreditCurve> BootstrapCreditCurve(
    std::span<const double> maturities, std::span<const double> spreads,
    int frequency, double recovery, const curves::ZeroCurve& discount) {
  if (maturities.empty() || maturities.size() != spreads.size()) {
    return std::nullopt;
  }
  std::vector<double> times;
  std::vector<double> hazards;
  for (std::size_t k = 0; k < maturities.size(); ++k) {
    if (maturities[k] <= 0.0 || (k > 0 && maturities[k] <= maturities[k - 1])) {
      return std::nullopt;
    }
    times.push_back(maturities[k]);
    hazards.push_back(0.0);
    const CdsTerms terms{.maturity = maturities[k],
                         .frequency = frequency,
                         .recovery = recovery};
    auto gap = [&](double h) {
      hazards.back() = h;
      const auto curve = curves::CreditCurve::Create(times, hazards);
      return CdsParSpread(terms, *curve, discount).value_or(0.0) - spreads[k];
    };
    const auto h = math::Brent(gap, 0.0, 10.0, {.tolerance = 1e-14});
    if (!h) return std::nullopt;
    hazards.back() = *h;
  }
  return curves::CreditCurve::Create(std::move(times), std::move(hazards));
}

double CdsOptionPrice(bool payer, double forward_spread, double strike,
                      double volatility, double expiry,
                      double forward_annuity) {
  const double s = volatility * std::sqrt(expiry);
  const double d1 = (std::log(forward_spread / strike) + 0.5 * s * s) / s;
  const double d2 = d1 - s;
  const double value =
      payer ? forward_spread * math::NormCdf(d1) - strike * math::NormCdf(d2)
            : strike * math::NormCdf(-d2) - forward_spread * math::NormCdf(-d1);
  return forward_annuity * value;
}

}  // namespace ofod::credit
