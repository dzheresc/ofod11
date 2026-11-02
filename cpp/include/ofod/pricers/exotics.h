// Copyright 2026 Denis Zhereshchin
//
// Closed forms for exotic options (Hull, Ch 26). Every function takes the
// market as spot, continuously compounded rate r and yield q, and a flat
// volatility; barriers and lookbacks assume continuous monitoring.

#ifndef OFOD_PRICERS_EXOTICS_H_
#define OFOD_PRICERS_EXOTICS_H_

#include <functional>
#include <optional>
#include <span>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"

namespace ofod::pricers {

using instruments::OptionType;

// Pays S_T - K1 if S_T > K2 (call) or K1 - S_T if S_T < K2 (put)
// (Ch 26.5).
[[nodiscard]] double GapOptionPrice(OptionType type, const TreeMarket& m,
                                    double strike, double trigger, double t);

// At-the-money option starting at t1 and expiring at t2 (Ch 26.6).
[[nodiscard]] double ForwardStartPrice(OptionType type, const TreeMarket& m,
                                       double t1, double t2);

// Perpetual American option (Ch 26.4). Returns std::nullopt for a call
// when q <= 0 (it is never exercised) or for r <= 0 with a put.
[[nodiscard]] std::optional<double> PerpetualAmericanPrice(OptionType type,
                                                           const TreeMarket& m,
                                                           double strike);

// Binary options (Ch 26.10).
[[nodiscard]] double CashOrNothingPrice(OptionType type, const TreeMarket& m,
                                        double strike, double t,
                                        double cash = 1.0);
[[nodiscard]] double AssetOrNothingPrice(OptionType type, const TreeMarket& m,
                                         double strike, double t);

enum class CompoundType { kCallOnCall, kPutOnCall, kCallOnPut, kPutOnPut };

// Geske's compound option (Ch 26.8): an option expiring at t1 with strike
// k1 on a vanilla expiring at t2 > t1 with strike k2. Returns
// std::nullopt when the critical spot cannot be bracketed.
[[nodiscard]] std::optional<double> CompoundOptionPrice(CompoundType type,
                                                        const TreeMarket& m,
                                                        double k1, double t1,
                                                        double k2, double t2);

// Simple chooser: call or put with strike K and expiry t2, chosen at t1
// (Ch 26.9).
[[nodiscard]] double ChooserPrice(const TreeMarket& m, double strike, double t1,
                                  double t2);

enum class BarrierType { kDownAndOut, kDownAndIn, kUpAndOut, kUpAndIn };

// Barrier option without rebate (Ch 26.9, Reiner-Rubinstein). A barrier
// already breached gives zero (out) or the vanilla price (in).
[[nodiscard]] double BarrierOptionPrice(BarrierType barrier, OptionType type,
                                        const TreeMarket& m, double strike,
                                        double level, double t);

// Floating-strike lookback (Ch 26.11): a call pays S_T - S_min, a put
// S_max - S_T. `extreme` is the minimum (call) or maximum (put) observed
// so far; S0 at inception. Returns std::nullopt when r == q.
[[nodiscard]] std::optional<double> FloatingLookbackPrice(OptionType type,
                                                          const TreeMarket& m,
                                                          double extreme,
                                                          double t);

// Fixed-strike lookback: a call pays max(S_max - K, 0), a put
// max(K - S_min, 0); `extreme` as above with max for a call, min for a put.
[[nodiscard]] std::optional<double> FixedLookbackPrice(OptionType type,
                                                       const TreeMarket& m,
                                                       double strike,
                                                       double extreme,
                                                       double t);

// Average-price option on the continuous geometric average (Ch 26.13).
[[nodiscard]] double GeometricAsianPrice(OptionType type, const TreeMarket& m,
                                         double strike, double t);

// Average-price option on the continuous arithmetic average, by matching
// the first two moments to a lognormal and using Black's model
// (eqs. 26.3, 26.4). Returns std::nullopt when r == q.
[[nodiscard]] std::optional<double> ArithmeticAsianPrice(OptionType type,
                                                         const TreeMarket& m,
                                                         double strike,
                                                         double t);

struct ExchangeAssets {
  double receive_spot;  // V0, the asset received.
  double deliver_spot;  // U0, the asset given up.
  double receive_yield = 0.0;
  double deliver_yield = 0.0;
  double receive_vol;
  double deliver_vol;
  double correlation;
};

// Option to exchange U for V at t, max(V_T - U_T, 0) (Margrabe, Ch 26.14).
[[nodiscard]] double ExchangeOptionPrice(const ExchangeAssets& a, double t);

using VolatilityFunction = std::function<double(double)>;

// Fair variance of a variance swap from out-of-the-money option prices on a
// strike grid (eq. 26.6): S* is the largest strike at or below F0, and the
// integral is the sum of dK_i / K_i^2 e^{rT} Q(K_i). Returns std::nullopt
// for fewer than 3 increasing strikes or F0 outside the grid.
[[nodiscard]] std::optional<double> VarianceSwapFairVariance(
    const TreeMarket& m, double t, std::span<const double> strikes,
    const VolatilityFunction& implied_vol);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_EXOTICS_H_
