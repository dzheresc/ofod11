// Copyright 2026 Denis Zhereshchin
//
// Chapter 7: valuing an interest rate swap as bonds and as FRAs, a currency
// swap, and comparative advantage.

#include <format>
#include <iostream>

#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/payoff.h"
#include "ofod/instruments/swap.h"
#include "ofod/pricers/swap_pricer.h"

int main() {
  namespace pricers = ofod::pricers;
  using ofod::instruments::Side;

  const auto curve =
      ofod::curves::ZeroCurve::Create({0.25, 0.75, 1.25}, {0.10, 0.105, 0.11});
  if (!curve) return 1;
  const ofod::instruments::InterestRateSwap swap{
      .notional = 100.0,
      .fixed_rate = 0.08,
      .frequency = 2,
      .payment_times = {0.25, 0.75, 1.25},
      .next_floating_rate = 0.102};
  std::cout << std::format(
      "IRS: B_fix {:.2f}, B_fl {:.2f}, value {:.2f} (as FRAs {:.2f})\n",
      pricers::FixedLegBondValue(swap, *curve),
      pricers::FloatingLegBondValue(swap, *curve),
      pricers::SwapValue(swap, *curve, Side::kLong),
      pricers::SwapValueAsFras(swap, *curve, Side::kLong));

  const ofod::instruments::CurrencySwap ccy{.domestic_notional = 10.0,
                                            .domestic_rate = 0.08,
                                            .foreign_notional = 1200.0,
                                            .foreign_rate = 0.05,
                                            .frequency = 1,
                                            .payment_times = {1.0, 2.0, 3.0}};
  std::cout << std::format(
      "Currency swap value: {:.4f} million USD\n",
      pricers::CurrencySwapValue(ccy, ofod::curves::ZeroCurve::Flat(0.09),
                                 ofod::curves::ZeroCurve::Flat(0.04),
                                 1.0 / 110.0, Side::kLong));
  std::cout << std::format(
      "Comparative advantage gain: {:.2f}%\n",
      100.0 * pricers::ComparativeAdvantageGain(0.04, 0.052, -0.001, 0.006));
  return 0;
}
