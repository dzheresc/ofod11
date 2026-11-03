// Copyright 2026 Denis Zhereshchin
//
// Standard market models for interest rate options (Hull, Ch 29): Black's
// model for bond options, caps, floors and swaptions, the normal
// (Bachelier) model, caplet volatility stripping and curve-bump hedging.
// Zero curves are continuously compounded; times are in years.

#ifndef OFOD_PRICERS_RATE_OPTIONS_H_
#define OFOD_PRICERS_RATE_OPTIONS_H_

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/option.h"
#include "ofod/pricers/fra_pricer.h"

namespace ofod::pricers {

using instruments::OptionType;

// Forward price of a bond for delivery at the option's expiry:
// (B0 - I) / P(0, T), where I is the present value of the coupons paid
// during the life of the option (eq. 29.1).
[[nodiscard]] double ForwardBondPrice(double spot_price, double coupon_pv,
                                      double discount);

// Black's model on the forward bond price (eqs. 29.1, 29.2):
// P(0, T) [F N(d1) - K N(d2)] for a call.
[[nodiscard]] double BlackBondOptionPrice(OptionType type, double forward_price,
                                          double strike, double discount,
                                          double volatility, double t);

// Forward bond price volatility from a yield volatility: D y0 sigma_y,
// where D is the modified duration of the forward bond (eq. 29.4).
[[nodiscard]] double PriceVolatilityFromYield(double modified_duration,
                                              double forward_yield,
                                              double yield_volatility);

// Undiscounted Black-76 value of an option on a forward with expiry `t`.
[[nodiscard]] double BlackFormula(OptionType type, double forward,
                                  double strike, double volatility, double t);

// Undiscounted normal-model (Bachelier) value with absolute volatility
// `normal_vol`; valid for zero and negative rates (Ch 29.4).
[[nodiscard]] double BachelierFormula(OptionType type, double forward,
                                      double strike, double normal_vol,
                                      double t);

// Caplet (kCall) or floorlet (kPut) on the rate reset at `reset` and paid
// `accrual` years later: L tau P(0, t_{k+1}) [F N(d1) - K N(d2)]
// (eqs. 29.7, 29.8).
[[nodiscard]] double BlackCapletPrice(OptionType type, double forward,
                                      double strike, double volatility,
                                      double reset, double accrual,
                                      double discount, double notional);

// A cap (kCall) or floor (kPut) on a rate with `frequency` resets a year.
// Caplets reset at k tau (k = 1, ..., n - 1, tau = 1 / frequency, n =
// maturity x frequency) and pay at (k + 1) tau; the first period's rate is
// known today and so carries no option, as in Hull.
struct CapTerms {
  double strike;
  double maturity;
  int frequency = 4;
  double notional = 1.0;
};

// Number of caplets in `terms`, or 0 if the maturity is not a multiple of
// the accrual period of at least two periods.
[[nodiscard]] int CapletCount(const CapTerms& terms);

// Black prices of each caplet; `volatilities` holds one spot volatility per
// caplet. Returns nullopt for a bad schedule or a size mismatch.
[[nodiscard]] std::optional<std::vector<double>> CapletPrices(
    OptionType type, const curves::ZeroCurve& curve, const CapTerms& terms,
    std::span<const double> volatilities);

// Cap or floor with the same (flat) volatility for every caplet.
[[nodiscard]] std::optional<double> CapFloorPrice(
    OptionType type, const curves::ZeroCurve& curve, const CapTerms& terms,
    double flat_volatility);

// Spot caplet volatilities from flat cap volatilities (Ch 29.2). The caps
// share `strike` and `frequency`; flat_vols[i] is quoted for the cap with
// i + 1 caplets, i.e. maturity (i + 2) tau. Each step solves for the vol
// of the new caplet that reproduces the next cap's price.
[[nodiscard]] std::optional<std::vector<double>> StripCapletVolatilities(
    const curves::ZeroCurve& curve, double strike, int frequency,
    std::span<const double> flat_vols, double notional = 1.0);

// Annuity A = sum tau P(0, t_i) and forward swap rate (P(0, T0) -
// P(0, Tn)) / A for a swap starting at `start` with `frequency` payments a
// year for `tenor` years.
struct ForwardSwap {
  double annuity;
  double rate;
};
[[nodiscard]] ForwardSwap ForwardSwapRate(const curves::ZeroCurve& curve,
                                          double start, double tenor,
                                          int frequency);

// Payer (kCall) or receiver (kPut) European swaption under Black's model:
// L A [s0 N(d1) - K N(d2)] (eqs. 29.10, 29.11).
[[nodiscard]] double BlackSwaptionPrice(OptionType type, double forward_rate,
                                        double strike, double volatility,
                                        double expiry, double annuity,
                                        double notional);

// The same swaption under the normal model.
[[nodiscard]] double BachelierSwaptionPrice(OptionType type,
                                            double forward_rate, double strike,
                                            double normal_vol, double expiry,
                                            double annuity, double notional);

// Value change of `value` when each pillar zero rate of `curve` is bumped
// by `bump` in turn (partial DV01s, Ch 29.5); their sum approximates the
// change for a parallel shift.
using CurvePricer = std::function<double(const curves::ZeroCurve&)>;
[[nodiscard]] std::vector<double> BucketDv01(const CurvePricer& value,
                                             const curves::ZeroCurve& curve,
                                             double bump = 1e-4);

// Value change for a parallel shift of all pillar rates by `bump`.
[[nodiscard]] double ParallelDv01(const CurvePricer& value,
                                  const curves::ZeroCurve& curve,
                                  double bump = 1e-4);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_RATE_OPTIONS_H_
