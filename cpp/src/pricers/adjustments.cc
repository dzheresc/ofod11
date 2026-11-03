// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/adjustments.h"

#include <cmath>

namespace ofod::pricers {

double ConvexityAdjustment(double forward_yield, double yield_volatility,
                           double t, double g_first, double g_second) {
  return -0.5 * forward_yield * forward_yield * yield_volatility *
         yield_volatility * t * g_second / g_first;
}

double BondYieldConvexityAdjustment(double forward_yield,
                                    double yield_volatility, double t,
                                    double coupon_rate, double maturity,
                                    int frequency) {
  // G(y) = sum_i c / m (1 + y/m)^-i + (1 + y/m)^-n.
  const double m = frequency;
  const int n = static_cast<int>(std::lround(maturity * m));
  const double base = 1.0 + forward_yield / m;
  double g1 = 0.0;
  double g2 = 0.0;
  for (int i = 1; i <= n; ++i) {
    const double cash = coupon_rate / m + (i == n ? 1.0 : 0.0);
    g1 += cash * -i / m * std::pow(base, -i - 1);
    g2 += cash * i * (i + 1) / (m * m) * std::pow(base, -i - 2);
  }
  return ConvexityAdjustment(forward_yield, yield_volatility, t, g1, g2);
}

double TimingAdjustmentFactor(double forward_rate, double forward_volatility,
                              double variable_volatility, double correlation,
                              double t, double tau) {
  return std::exp(-correlation * variable_volatility * forward_volatility *
                  forward_rate * tau * t / (1.0 + forward_rate * tau));
}

double QuantoAdjustmentFactor(double correlation, double exchange_volatility,
                              double variable_volatility, double t) {
  return std::exp(correlation * exchange_volatility * variable_volatility * t);
}

double InArrearsRate(double forward_rate, double volatility, double t,
                     double tau) {
  return forward_rate + forward_rate * forward_rate * volatility * volatility *
                            tau * t / (1.0 + forward_rate * tau);
}

double CmsRate(const CmsTerms& terms) {
  const double adjusted =
      terms.forward_swap_rate +
      BondYieldConvexityAdjustment(
          terms.forward_swap_rate, terms.swap_volatility, terms.t,
          terms.forward_swap_rate, terms.swap_tenor, terms.frequency);
  if (terms.delay <= 0.0) return adjusted;
  return adjusted *
         TimingAdjustmentFactor(terms.delay_rate, terms.delay_volatility,
                                terms.swap_volatility, terms.correlation,
                                terms.t, terms.delay);
}

}  // namespace ofod::pricers
