// Copyright 2026 Denis Zhereshchin
//
// Chapter 20: interpolating Hull's volatility surface (Table 20.2), a
// smile quoted in delta space, the implied risk-neutral density of a skewed
// smile, and the volatility frown produced by a single large jump.

#include <cmath>
#include <format>
#include <iostream>
#include <vector>

#include "ofod/curves/vol_surface.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/smile.h"

namespace {

namespace p = ofod::pricers;
using ofod::curves::VolInterpolation;
using ofod::curves::VolSurface;
using ofod::instruments::OptionType;

void PrintSurface() {
  const std::vector<std::vector<double>> vols = {
      {0.142, 0.130, 0.120, 0.131, 0.145}, {0.140, 0.130, 0.120, 0.131, 0.142},
      {0.141, 0.133, 0.125, 0.134, 0.143}, {0.147, 0.140, 0.135, 0.140, 0.148},
      {0.150, 0.144, 0.140, 0.145, 0.151}, {0.148, 0.146, 0.144, 0.147, 0.150}};
  const std::vector<double> maturities = {1.0 / 12.0, 0.25, 0.5, 1.0, 2.0, 5.0};
  const std::vector<double> strikes = {0.90, 0.95, 1.00, 1.05, 1.10};
  const auto linear = VolSurface::Create(maturities, strikes, vols,
                                         VolInterpolation::kLinearVol);
  const auto variance = VolSurface::Create(maturities, strikes, vols);
  if (!linear || !variance) return;
  std::cout << "Table 20.2 surface, K/S0 = 1.05, 9 months\n";
  std::cout << std::format(
      "  linear in vol: {:.2f}%   linear in total variance: {:.2f}%\n",
      100.0 * linear->Volatility(1.05, 0.75),
      100.0 * variance->Volatility(1.05, 0.75));
  std::cout << "  term structure at K/S0 = 1.00:";
  for (double t : {0.1, 0.75, 1.5, 3.0, 7.0}) {
    std::cout << std::format(" {:.2f}%", 100.0 * variance->Volatility(1.0, t));
  }
  std::cout << '\n';
}

// A skewed equity smile: sigma(K) = 0.25 - 0.1 (K / S0 - 1).
double SkewVol(double strike) { return 0.25 - 0.1 * (strike / 100.0 - 1.0); }

void PrintDeltaSpaceAndDensity() {
  std::cout << "Strikes by delta, S0 = 100, r = 3%, q = 1%, T = 0.5, vol 25%\n";
  for (double d : {0.10, 0.25, 0.50, 0.75, 0.90}) {
    if (const auto k = p::StrikeFromDelta(OptionType::kCall, d, 100.0, 0.03,
                                          0.01, 0.25, 0.5)) {
      std::cout << std::format("  {:>2.0f}-delta call: K = {:.2f}\n", 100.0 * d,
                               *k);
    }
  }
  std::cout << "Implied density of the skewed smile vs lognormal (T = 0.5)\n";
  auto call = [](double k) {
    return p::BsmPrice(OptionType::kCall, 100.0, k, 0.03, 0.0, SkewVol(k), 0.5);
  };
  constexpr double kSpacing = 0.5;
  for (double k = 70.0; k <= 130.0; k += 10.0) {
    const double g = p::ImpliedDensity(call(k - kSpacing), call(k),
                                       call(k + kSpacing), kSpacing, 0.03, 0.5);
    std::cout << std::format(
        "  K = {:>3.0f}: implied {:.5f}  lognormal(25%) {:.5f}\n", k, g,
        p::LognormalDensity(100.0, k, 0.03, 0.0, 0.25, 0.5));
  }
}

void PrintJump() {
  std::cout << "Single large jump: S = 50 moves to 42 or 58 in one month\n";
  for (double k = 44.0; k <= 56.0; k += 2.0) {
    const double price = p::BinaryJumpPrice(OptionType::kCall, k, 0.0,
                                            1.0 / 12.0, 58.0, 42.0, 0.5);
    if (const auto iv = p::ImpliedVolatility(OptionType::kCall, price, 50.0, k,
                                             0.0, 0.0, 1.0 / 12.0)) {
      std::cout << std::format(
          "  K = {:.0f}: call {:.2f}, implied vol {:.1f}%\n", k, price,
          100.0 * *iv);
    }
  }
}

}  // namespace

int main() {
  PrintSurface();
  PrintDeltaSpaceAndDensity();
  PrintJump();
  return 0;
}
