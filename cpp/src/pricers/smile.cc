// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/smile.h"

#include <cmath>
#include <numbers>
#include <optional>

#include "ofod/instruments/option.h"
#include "ofod/math/normal.h"

namespace ofod::pricers {

using instruments::OptionType;

std::optional<double> StrikeFromDelta(OptionType type, double delta,
                                      double spot, double rate, double yield,
                                      double volatility, double t) {
  const double scale = std::exp(yield * t);
  const double n_d1 =
      type == OptionType::kCall ? delta * scale : 1.0 + delta * scale;
  if (!(n_d1 > 0.0 && n_d1 < 1.0)) return std::nullopt;
  const double d1 = math::NormInv(n_d1);
  const double sqrt_t = std::sqrt(t);
  return spot * std::exp(-d1 * volatility * sqrt_t +
                         (rate - yield + 0.5 * volatility * volatility) * t);
}

double MinimumVarianceDelta(double bsm_delta, double bsm_vega, double spot,
                            double t, double a, double b, double c) {
  return bsm_delta + bsm_vega / (spot * std::sqrt(t)) *
                         (a + b * bsm_delta + c * bsm_delta * bsm_delta);
}

double ImpliedDensity(double call_low, double call_mid, double call_high,
                      double strike_spacing, double rate, double t) {
  return std::exp(rate * t) * (call_low + call_high - 2.0 * call_mid) /
         (strike_spacing * strike_spacing);
}

double LognormalDensity(double spot, double strike, double rate, double yield,
                        double volatility, double t) {
  const double s = volatility * std::sqrt(t);
  const double m =
      std::log(spot) + (rate - yield - 0.5 * volatility * volatility) * t;
  const double z = (std::log(strike) - m) / s;
  return std::exp(-0.5 * z * z) /
         (strike * s * std::sqrt(2.0 * std::numbers::pi));
}

double BinaryJumpPrice(OptionType type, double strike, double rate, double t,
                       double up, double down, double p_up) {
  auto payoff = [&](double s) {
    return type == OptionType::kCall ? std::max(s - strike, 0.0)
                                     : std::max(strike - s, 0.0);
  };
  return std::exp(-rate * t) *
         (p_up * payoff(up) + (1.0 - p_up) * payoff(down));
}

}  // namespace ofod::pricers
