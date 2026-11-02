// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/swap_pricer.h"

#include <cmath>

namespace ofod::pricers {

using instruments::CurrencySwap;
using instruments::InterestRateSwap;

double FixedLegBondValue(const InterestRateSwap& s,
                         const curves::ZeroCurve& curve) {
  double value = 0.0;
  for (double t : s.payment_times) {
    value += s.notional * s.fixed_rate / s.frequency * curve.DiscountFactor(t);
  }
  return value + s.notional * curve.DiscountFactor(s.payment_times.back());
}

double FloatingLegBondValue(const InterestRateSwap& s,
                            const curves::ZeroCurve& curve) {
  const double next = s.notional * s.next_floating_rate / s.frequency;
  return (s.notional + next) * curve.DiscountFactor(s.payment_times.front());
}

double SwapValue(const InterestRateSwap& s, const curves::ZeroCurve& curve,
                 instruments::Side side) {
  return instruments::Sign(side) *
         (FixedLegBondValue(s, curve) - FloatingLegBondValue(s, curve));
}

double SwapValueAsFras(const InterestRateSwap& s,
                       const curves::ZeroCurve& curve, instruments::Side side) {
  const double tau = 1.0 / s.frequency;
  const auto& times = s.payment_times;
  double value = s.notional * (s.fixed_rate - s.next_floating_rate) * tau *
                 curve.DiscountFactor(times.front());
  for (std::size_t i = 1; i < times.size(); ++i) {
    const double df_prev = curve.DiscountFactor(times[i - 1]);
    const double df = curve.DiscountFactor(times[i]);
    const double forward = (df_prev / df - 1.0) / tau;
    value += s.notional * (s.fixed_rate - forward) * tau * df;
  }
  return instruments::Sign(side) * value;
}

double ParSwapRate(std::span<const double> payment_times, int frequency,
                   const curves::ZeroCurve& curve) {
  double annuity = 0.0;
  for (double t : payment_times) annuity += curve.DiscountFactor(t) / frequency;
  return (1.0 - curve.DiscountFactor(payment_times.back())) / annuity;
}

double CurrencySwapValue(const CurrencySwap& s,
                         const curves::ZeroCurve& domestic,
                         const curves::ZeroCurve& foreign, double spot,
                         instruments::Side side) {
  auto bond = [&](double notional, double rate,
                  const curves::ZeroCurve& curve) {
    double value = 0.0;
    for (double t : s.payment_times) {
      value += notional * rate / s.frequency * curve.DiscountFactor(t);
    }
    return value + notional * curve.DiscountFactor(s.payment_times.back());
  };
  const double b_domestic =
      bond(s.domestic_notional, s.domestic_rate, domestic);
  const double b_foreign = bond(s.foreign_notional, s.foreign_rate, foreign);
  return instruments::Sign(side) * (spot * b_foreign - b_domestic);
}

double ComparativeAdvantageGain(double fixed_a, double fixed_b,
                                double floating_spread_a,
                                double floating_spread_b) {
  return std::abs(fixed_b - fixed_a) -
         std::abs(floating_spread_b - floating_spread_a);
}

}  // namespace ofod::pricers
