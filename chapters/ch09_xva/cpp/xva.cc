// Copyright 2026 Denis Zhereshchin
//
// Chapter 9: exposure profiles of a forward by Monte Carlo, CVA with and
// without a collateral threshold, DVA and FVA.

#include "ofod/credit/xva.h"

#include <format>
#include <iostream>
#include <vector>

#include "ofod/curves/credit_curve.h"
#include "ofod/instruments/option.h"
#include "ofod/pricers/black_scholes.h"

namespace {

namespace c = ofod::credit;
using ofod::curves::CreditCurve;

void Print(const char* title, const c::ExposureProfile& p) {
  std::cout << title << '\n' << "  time      EE     ENE   PFE97.5\n";
  for (std::size_t j = 0; j < p.times.size(); ++j) {
    std::cout << std::format("  {:4.2f} {:7.2f} {:7.2f} {:9.2f}\n", p.times[j],
                             p.expected_exposure[j],
                             p.expected_negative_exposure[j],
                             p.potential_future_exposure[j]);
  }
}

}  // namespace

int main() {
  // CVA on discrete intervals with a flat 2% hazard, 40% recovery.
  const std::vector<double> times = {1.0, 2.0, 3.0};
  const std::vector<double> ee = {10.0, 8.0, 5.0};
  const auto counterparty = CreditCurve::Flat(0.02);
  const auto cva = c::Cva(times, ee, counterparty, 0.4);
  const auto fva = c::FundingValueAdjustment(times, ee, 0.005);
  std::cout << std::format(
      "Exposures (10, 8, 5) at 1-3 years: CVA {:.4f}, FVA at 50 bp {:.4f}\n",
      cva.value_or(0.0), fva.value_or(0.0));

  // Long forward on an asset at 1600, K = 1500, r = 5%, vol 20%, T = 1.
  // Default only at T: exposure is a call on the asset (Hull, Ch 9.8).
  const double call =
      ofod::pricers::BsmPrice(ofod::instruments::OptionType::kCall, 1600.0,
                              1500.0, 0.05, 0.0, 0.2, 1.0);
  const auto forward_cva = c::Cva(std::vector<double>{1.0},
                                  std::vector<double>{call}, counterparty, 0.3);
  std::cout << std::format(
      "Forward, default only at maturity: EE {:.4f}, CVA {:.4f}\n", call,
      forward_cva.value_or(0.0));

  const auto plain =
      c::SimulateForwardExposure(1600.0, 1500.0, 0.05, 0.2, 1.0, 4, 100'000, 3);
  const auto capped = c::SimulateForwardExposure(1600.0, 1500.0, 0.05, 0.2, 1.0,
                                                 4, 100'000, 3, 0.975, 100.0);
  if (!plain || !capped) return 1;
  Print("Simulated forward exposure (discounted, 100,000 paths)", *plain);
  Print("With a collateral threshold of 100", *capped);
  const auto own = CreditCurve::Flat(0.01);
  const auto cva_mc =
      c::Cva(plain->times, plain->expected_exposure, counterparty, 0.3);
  const auto cva_capped =
      c::Cva(capped->times, capped->expected_exposure, counterparty, 0.3);
  const auto dva =
      c::Dva(plain->times, plain->expected_negative_exposure, own, 0.4);
  std::cout << std::format(
      "Quarterly default: CVA {:.4f}, with threshold {:.4f}, DVA (own hazard "
      "1%) {:.4f}\n",
      cva_mc.value_or(0.0), cva_capped.value_or(0.0), dva.value_or(0.0));
  return 0;
}
