// Copyright 2026 Denis Zhereshchin
//
// Chapter 34: amortizing, step-up and forward swaps, LIBOR-in-arrears and
// CMS swaps with their convexity adjustments, a diff swap, an accrual swap
// and a cancelable swap valued as a swap plus a Bermudan swaption on a
// Hull-White tree.

#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/pricers/adjustments.h"
#include "ofod/pricers/exotic_swaps.h"
#include "ofod/pricers/fra_pricer.h"
#include "ofod/pricers/rate_options.h"
#include "ofod/pricers/short_rate_tree.h"

namespace {

namespace p = ofod::pricers;
using ofod::curves::ZeroCurve;
using ofod::instruments::OptionType;

// Unwraps a result the example inputs guarantee; main reports a failure.
template <typename T>
T Must(std::optional<T> value) {
  if (!value) throw std::runtime_error("unexpected pricing failure");
  return *std::move(value);
}

ZeroCurve Upward() {
  return Must(ZeroCurve::Create({0.25, 1.0, 2.0, 5.0, 10.0},
                                {0.03, 0.035, 0.04, 0.045, 0.05}));
}

void Variations(const ZeroCurve& curve) {
  const auto plain = p::SwapSchedule(0.0, 5.0, 2, 100.0, 0.0);
  const auto amortizing = p::SwapSchedule(0.0, 5.0, 2, 100.0, 0.0, 10.0);
  const auto forward = p::SwapSchedule(2.0, 7.0, 2, 100.0, 0.0);
  std::cout << std::format(
      "Par fixed rates, 5-year semiannual: plain {:.4f}%, amortizing by 10 "
      "a period {:.4f}%, starting in 2 years {:.4f}%\n",
      100.0 * p::ParFixedRate(curve, plain),
      100.0 * p::ParFixedRate(curve, amortizing),
      100.0 * p::ParFixedRate(curve, forward));
  const auto step_up = p::SwapSchedule(0.0, 5.0, 2, 100.0, 0.04, 0.0, 0.001);
  std::cout << std::format(
      "Step-up swap, receive 4% rising 10 bp a period on 100: value {:.4f}\n",
      p::ScheduleSwapValue(curve, step_up));
}

void Adjusted(const ZeroCurve& curve) {
  const auto periods = p::SwapSchedule(0.0, 5.0, 2, 100.0, 0.045);
  const std::vector<double> vols(periods.size(), 0.2);
  std::cout << std::format(
      "Receive 4.5% on 100 for 5 years: standard {:.4f}, LIBOR-in-arrears "
      "(20% vol) {:.4f}\n",
      p::ScheduleSwapValue(curve, periods),
      p::InArrearsSwapValue(curve, periods, vols));
  const double f = p::SimpleForwardRate(curve, 3.0, 3.5);
  std::cout << std::format(
      "  in-arrears adjustment for the rate reset at 3 years: {:.4f}% -> "
      "{:.4f}%\n",
      100.0 * f, 100.0 * p::InArrearsRate(f, 0.2, 3.0, 0.5));
  const p::CmsLeg leg{.cms_tenor = 5.0,
                      .cms_frequency = 2,
                      .forward_vol = 0.18,
                      .correlation = 0.7};
  const auto swap = p::ForwardSwapRate(curve, 3.0, 5.0, 2);
  const double cms =
      p::CmsRate({.forward_swap_rate = swap.rate,
                  .swap_volatility = 0.2,
                  .t = 3.0,
                  .swap_tenor = 5.0,
                  .frequency = 2,
                  .delay = 0.5,
                  .delay_rate = p::SimpleForwardRate(curve, 3.0, 3.5),
                  .delay_volatility = 0.18,
                  .correlation = 0.7});
  std::cout << std::format(
      "CMS swap, receive 4.5% against the 5-year swap rate: {:.4f}; the "
      "rate set at 3 years is {:.4f}% forward, {:.4f}% adjusted\n",
      p::CmsSwapValue(curve, periods, leg, vols), 100.0 * swap.rate,
      100.0 * cms);
}

void DiffAndAccrual(const ZeroCurve& curve) {
  // A diff swap pays a foreign rate on a domestic notional: the foreign
  // forward (4%, 20% vol) is quanto-adjusted with an exchange rate vol of
  // 10% and correlation 0.3.
  std::cout << "Diff swap: quanto-adjusted foreign forwards (%):";
  for (double t : {1.0, 2.0, 5.0}) {
    std::cout << std::format(
        " {:.0f}y {:.4f}", t,
        100.0 * 0.04 * p::QuantoAdjustmentFactor(0.3, 0.1, 0.2, t));
  }
  std::cout << '\n';
  const auto periods = p::SwapSchedule(0.0, 3.0, 4, 100.0, 0.05);
  double full = 0.0;
  for (const auto& q : periods) {
    full += q.notional * q.fixed_rate * 0.25 * curve.DiscountFactor(q.end);
  }
  std::cout << std::format(
      "Accrual swap, 5% accruing while the 3-month rate is in [3%, 5%] "
      "(20% vol, 30 fixings a quarter): fixed leg {:.4f} of {:.4f}\n",
      p::AccrualFixedLegValue(curve, periods, 0.03, 0.05, 0.2, 30), full);
}

void Cancelable(const ZeroCurve& curve) {
  // Receive 5% annually for 5 years with the right to cancel at years 1-4:
  // the receiver owns a Bermudan payer swaption.
  const auto tree = p::ShortRateTree::Build(curve, 0.1, 0.01, 0.05, 100);
  if (!tree) return;
  const auto periods = p::SwapSchedule(0.0, 5.0, 1, 1.0, 0.05);
  const double swap = 100.0 * p::ScheduleSwapValue(curve, periods);
  const std::vector<int> dates = {20, 40, 60, 80};
  const double bermudan = 100.0 * Must(p::TreeSwaption(*tree, OptionType::kCall,
                                                       0.05, 20, 100, dates));
  const double european =
      100.0 * Must(p::TreeSwaption(*tree, OptionType::kCall, 0.05, 20, 100,
                                   std::vector<int>{20}));
  std::cout << std::format(
      "Cancelable swap on 100: receive-fixed swap {:.4f} + Bermudan payer "
      "swaption {:.4f} = {:.4f} (cancel only at year 1: {:.4f})\n",
      swap, bermudan, swap + bermudan, swap + european);
}

}  // namespace

int main() {
  try {
    const ZeroCurve curve = Upward();
    Variations(curve);
    Adjusted(curve);
    DiffAndAccrual(curve);
    Cancelable(curve);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
