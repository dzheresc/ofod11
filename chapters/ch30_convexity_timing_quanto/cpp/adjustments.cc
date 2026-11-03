// Copyright 2026 Denis Zhereshchin
//
// Chapter 30: convexity adjustments for bond yields and swap rates, the
// timing adjustment, quanto adjustments and Siegel's paradox.

#include "ofod/pricers/adjustments.h"

#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <numbers>

#include "ofod/random/normal_generator.h"

namespace {

namespace p = ofod::pricers;

void BookExamples() {
  const double adj =
      p::BondYieldConvexityAdjustment(0.1, 0.2, 3.0, 0.0, 1.0, 1);
  std::cout << std::format(
      "Example 30.1: 1-year rate in 3 years: adjustment {:.4f}%, expected "
      "rate {:.4f}%, value {:.2f}\n",
      100.0 * adj, 100.0 * (0.1 + adj),
      1000.0 * (0.1 + adj) / std::pow(1.1, 3));
  const double factor = p::TimingAdjustmentFactor(0.1, 0.2, 0.2, 1.0, 3.0, 1.0);
  const double rate = (0.1 + adj) * factor;
  std::cout << std::format(
      "Example 30.3: paid in 4 years instead: timing factor {:.6f}, expected "
      "rate {:.4f}%, value {:.2f}\n",
      factor, 100.0 * rate, 1000.0 * rate / std::pow(1.1, 4));
  const double swap =
      p::BondYieldConvexityAdjustment(0.12, 0.22, 4.0, 0.12, 3.0, 1);
  std::cout << std::format(
      "3-year swap rate in 4 years, forward 12%, 22% vol: adjustment {:.4f}%\n",
      100.0 * swap);
}

void AdjustmentTable() {
  std::cout << "Adjustments (bp) by observation time T, 5-year annual swap "
               "rate at 5%\n"
               "      T  convexity 15%  convexity 30%  quanto rho=-0.3\n";
  for (double t : {1.0, 2.0, 5.0, 10.0, 20.0}) {
    const double c15 =
        p::BondYieldConvexityAdjustment(0.05, 0.15, t, 0.05, 5.0, 1);
    const double c30 =
        p::BondYieldConvexityAdjustment(0.05, 0.30, t, 0.05, 5.0, 1);
    const double quanto =
        0.05 * (p::QuantoAdjustmentFactor(-0.3, 0.12, 0.15, t) - 1.0);
    std::cout << std::format("  {:5.0f}  {:13.2f}  {:13.2f}  {:15.2f}\n", t,
                             1e4 * c15, 1e4 * c30, 1e4 * quanto);
  }
}

void SiegelParadox() {
  // S is the USD price of 1 EUR with forward F. Under the USD measure S_T
  // has mean F; the EUR price of 1 USD, 1 / S_T, then has mean (1 / F)
  // e^{sigma^2 T}, not 1 / F. Under the EUR measure (a quanto change of
  // drift by sigma^2) the mean of 1 / S_T is 1 / F, as it must be.
  const double f = 1.10;
  const double sigma = 0.15;
  const double t = 2.0;
  const double root_t = std::numbers::sqrt2;  // sqrt(t) for t = 2.
  ofod::random::NormalGenerator rng(7);
  constexpr int kPaths = 200'000;
  double usd_s = 0.0;
  double usd_inverse = 0.0;
  double eur_inverse = 0.0;
  for (int i = 0; i < kPaths; ++i) {
    const double z = rng.Next();
    const double s =
        f * std::exp(-0.5 * sigma * sigma * t + sigma * root_t * z);
    usd_s += s;
    usd_inverse += 1.0 / s;
    eur_inverse += 1.0 / (s * std::exp(sigma * sigma * t));
  }
  std::cout << std::format(
      "Siegel's paradox, F = {:.2f}, 15% vol, 2 years: USD measure E[S] = "
      "{:.4f}, E[1/S] = {:.4f} vs 1/F = {:.4f} (theory {:.4f}); EUR measure "
      "E[1/S] = {:.4f}\n",
      f, usd_s / kPaths, usd_inverse / kPaths, 1.0 / f,
      std::exp(sigma * sigma * t) / f, eur_inverse / kPaths);
}

}  // namespace

int main() {
  try {
    BookExamples();
    AdjustmentTable();
    SiegelParadox();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
