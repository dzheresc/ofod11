// Copyright 2026 Denis Zhereshchin
//
// Nonstandard swaps (Hull, Ch 34): swaps with varying notionals and rates
// (amortizing, step-up, forward start), LIBOR-in-arrears, constant
// maturity swaps and accrual swaps. Values are to the receiver of the
// fixed leg and assume, as Hull does, that forward rates are realised
// after any convexity and timing adjustments.

#ifndef OFOD_PRICERS_EXOTIC_SWAPS_H_
#define OFOD_PRICERS_EXOTIC_SWAPS_H_

#include <span>
#include <vector>

#include "ofod/curves/zero_curve.h"

namespace ofod::pricers {

// One accrual period: the floating rate for [start, end] is set at
// `start` and paid at `end` on `notional`, against `fixed_rate`.
struct SwapPeriod {
  double start;
  double end;
  double notional;
  double fixed_rate;
};

// Regular schedule from `start` to `maturity` with `frequency` periods a
// year. The notional changes by `amortization` (subtracted) per period and
// the fixed rate by `step_up` (added) per period.
[[nodiscard]] std::vector<SwapPeriod> SwapSchedule(
    double start, double maturity, int frequency, double notional,
    double fixed_rate, double amortization = 0.0, double step_up = 0.0);

// sum N tau (K - F) P(0, end) with F the simple forward for each period.
[[nodiscard]] double ScheduleSwapValue(const curves::ZeroCurve& curve,
                                       std::span<const SwapPeriod> periods);

// Fixed rate that gives the schedule's notionals a zero value.
[[nodiscard]] double ParFixedRate(const curves::ZeroCurve& curve,
                                  std::span<const SwapPeriod> periods);

// LIBOR-in-arrears swap: the rate for [end, end + tau] is set and paid at
// `end` (eq. 34.1), with vols[i] the caplet volatility of period i's rate.
[[nodiscard]] double InArrearsSwapValue(const curves::ZeroCurve& curve,
                                        std::span<const SwapPeriod> periods,
                                        std::span<const double> vols);

// Constant maturity swap: the floating leg pays the `cms_tenor` swap rate
// (with `cms_frequency` payments) set at `start` and paid at `end`;
// swap_vols[i] is the volatility of period i's forward swap rate. The
// timing adjustment uses the period's forward rate with volatility
// `forward_vol` and correlation `correlation` (Ch 34.4).
struct CmsLeg {
  double cms_tenor;
  int cms_frequency;
  double forward_vol;
  double correlation = 1.0;
};
[[nodiscard]] double CmsSwapValue(const curves::ZeroCurve& curve,
                                  std::span<const SwapPeriod> periods,
                                  const CmsLeg& leg,
                                  std::span<const double> swap_vols);

// Fixed leg of an accrual swap (Ch 34.6): the fixed rate accrues only on
// observation dates when the reference rate (the simple rate for a period
// of the same length) is in [low, high]. Each period has `observations`
// equally spaced observation dates; the probability of being in range is
// N(d2(low)) - N(d2(high)) under Black with volatility `vol`, and every
// accrual is paid at the period end.
[[nodiscard]] double AccrualFixedLegValue(const curves::ZeroCurve& curve,
                                          std::span<const SwapPeriod> periods,
                                          double low, double high, double vol,
                                          int observations);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_EXOTIC_SWAPS_H_
