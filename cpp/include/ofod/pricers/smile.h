// Copyright 2026 Denis Zhereshchin
//
// Volatility smile tools (Hull, Ch 20): strikes from deltas, minimum
// variance delta, the implied risk-neutral density and the price of an
// option when a single large jump is expected.

#ifndef OFOD_PRICERS_SMILE_H_
#define OFOD_PRICERS_SMILE_H_

#include <optional>

#include "ofod/instruments/option.h"

namespace ofod::pricers {

// Strike whose BSM delta equals `delta` (Ch 20.6: a smile quoted in delta
// space). Call deltas lie in (0, e^{-qT}), put deltas in (-e^{-qT}, 0);
// returns std::nullopt outside that range.
[[nodiscard]] std::optional<double> StrikeFromDelta(
    instruments::OptionType type, double delta, double spot, double rate,
    double yield, double volatility, double t);

// delta_MV = delta_BS + vega_BS / (S sqrt(T)) (a + b delta_BS + c delta_BS^2)
// (Hull and White's minimum variance delta, Ch 20.5).
[[nodiscard]] double MinimumVarianceDelta(double bsm_delta, double bsm_vega,
                                          double spot, double t, double a,
                                          double b, double c);

// Breeden-Litzenberger (Appendix to Ch 20): g(K) = e^{rT} (c1 + c3 - 2 c2)
// / delta^2 from calls struck at K - delta, K and K + delta.
[[nodiscard]] double ImpliedDensity(double call_low, double call_mid,
                                    double call_high, double strike_spacing,
                                    double rate, double t);

// Lognormal risk-neutral density of S_T at `strike` under BSM, for
// comparison with the implied density.
[[nodiscard]] double LognormalDensity(double spot, double strike, double rate,
                                      double yield, double volatility,
                                      double t);

// The stock will be at `up` or `down` at T with risk-neutral probability
// `p_up` (Ch 20.8): the option is worth e^{-rT} E[payoff].
[[nodiscard]] double BinaryJumpPrice(instruments::OptionType type,
                                     double strike, double rate, double t,
                                     double up, double down, double p_up);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_SMILE_H_
