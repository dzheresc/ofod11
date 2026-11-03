// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/exotic_swaps.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "ofod/math/normal.h"
#include "ofod/pricers/adjustments.h"
#include "ofod/pricers/fra_pricer.h"
#include "ofod/pricers/rate_options.h"

namespace ofod::pricers {
namespace {

// Probability under Black that the forward ends above `strike`: N(d2).
double ProbabilityAbove(double forward, double strike, double vol, double t) {
  if (strike <= 0.0) return 1.0;
  const double sd = vol * std::sqrt(t);
  if (sd <= 0.0) return forward > strike ? 1.0 : 0.0;
  return math::NormCdf(std::log(forward / strike) / sd - 0.5 * sd);
}

}  // namespace

std::vector<SwapPeriod> SwapSchedule(double start, double maturity,
                                     int frequency, double notional,
                                     double fixed_rate, double amortization,
                                     double step_up) {
  std::vector<SwapPeriod> out;
  const double tau = 1.0 / frequency;
  const int n = static_cast<int>(std::lround((maturity - start) * frequency));
  out.reserve(static_cast<std::size_t>(std::max(n, 0)));
  for (int i = 0; i < n; ++i) {
    out.push_back({.start = start + i * tau,
                   .end = start + (i + 1) * tau,
                   .notional = notional - i * amortization,
                   .fixed_rate = fixed_rate + i * step_up});
  }
  return out;
}

double ScheduleSwapValue(const curves::ZeroCurve& curve,
                         std::span<const SwapPeriod> periods) {
  double value = 0.0;
  for (const SwapPeriod& p : periods) {
    const double tau = p.end - p.start;
    const double forward = SimpleForwardRate(curve, p.start, p.end);
    value += p.notional * tau * (p.fixed_rate - forward) *
             curve.DiscountFactor(p.end);
  }
  return value;
}

double ParFixedRate(const curves::ZeroCurve& curve,
                    std::span<const SwapPeriod> periods) {
  double floating = 0.0;
  double annuity = 0.0;
  for (const SwapPeriod& p : periods) {
    const double tau = p.end - p.start;
    const double df = curve.DiscountFactor(p.end);
    floating +=
        p.notional * tau * SimpleForwardRate(curve, p.start, p.end) * df;
    annuity += p.notional * tau * df;
  }
  return floating / annuity;
}

double InArrearsSwapValue(const curves::ZeroCurve& curve,
                          std::span<const SwapPeriod> periods,
                          std::span<const double> vols) {
  double value = 0.0;
  for (std::size_t i = 0; i < periods.size(); ++i) {
    const SwapPeriod& p = periods[i];
    const double tau = p.end - p.start;
    const double forward = SimpleForwardRate(curve, p.end, p.end + tau);
    const double rate = InArrearsRate(forward, vols[i], p.end, tau);
    value +=
        p.notional * tau * (p.fixed_rate - rate) * curve.DiscountFactor(p.end);
  }
  return value;
}

double CmsSwapValue(const curves::ZeroCurve& curve,
                    std::span<const SwapPeriod> periods, const CmsLeg& leg,
                    std::span<const double> swap_vols) {
  double value = 0.0;
  for (std::size_t i = 0; i < periods.size(); ++i) {
    const SwapPeriod& p = periods[i];
    const double tau = p.end - p.start;
    const ForwardSwap swap =
        ForwardSwapRate(curve, p.start, leg.cms_tenor, leg.cms_frequency);
    // The forward swap rate is quoted with the swap's own compounding.
    const CmsTerms terms{.forward_swap_rate = swap.rate,
                         .swap_volatility = swap_vols[i],
                         .t = p.start,
                         .swap_tenor = leg.cms_tenor,
                         .frequency = leg.cms_frequency,
                         .delay = tau,
                         .delay_rate = SimpleForwardRate(curve, p.start, p.end),
                         .delay_volatility = leg.forward_vol,
                         .correlation = leg.correlation};
    value += p.notional * tau * (p.fixed_rate - CmsRate(terms)) *
             curve.DiscountFactor(p.end);
  }
  return value;
}

double AccrualFixedLegValue(const curves::ZeroCurve& curve,
                            std::span<const SwapPeriod> periods, double low,
                            double high, double vol, int observations) {
  double value = 0.0;
  for (const SwapPeriod& p : periods) {
    const double tau = p.end - p.start;
    double in_range = 0.0;
    for (int k = 1; k <= observations; ++k) {
      const double t = p.start + tau * k / observations;
      const double forward = SimpleForwardRate(curve, t, t + tau);
      in_range += ProbabilityAbove(forward, low, vol, t) -
                  ProbabilityAbove(forward, high, vol, t);
    }
    value += p.notional * p.fixed_rate * tau * in_range / observations *
             curve.DiscountFactor(p.end);
  }
  return value;
}

}  // namespace ofod::pricers
