// Copyright 2026 Denis Zhereshchin
//
// Chapter 28: the market price of risk, and one price computed under three
// numeraires (money market, the stock itself, a zero-coupon bond), plus an
// exchange option valued with the delivered asset as numeraire.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>

#include "ofod/greeks/greeks.h"
#include "ofod/math/normal.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/exotics.h"
#include "ofod/random/normal_generator.h"

namespace {

namespace p = ofod::pricers;
using ofod::instruments::OptionType;

constexpr int kPaths = 100'000;

struct Estimate {
  double mean;
  double se;
};

template <class F>
Estimate Simulate(std::uint64_t seed, F&& draw) {
  ofod::random::NormalGenerator rng(seed);
  double sum = 0.0;
  double sum_sq = 0.0;
  for (int i = 0; i < kPaths; ++i) {
    const double x = draw(rng);
    sum += x;
    sum_sq += x * x;
  }
  const double mean = sum / kPaths;
  return {mean, std::sqrt((sum_sq / kPaths - mean * mean) / (kPaths - 1))};
}

void MarketPriceOfRisk() {
  // A stock with mu = 12%, sigma = 20%, r = 5%, and a 1-year ATM call on it.
  const double mu = 0.12;
  const double sigma = 0.2;
  const double r = 0.05;
  const double lambda = (mu - r) / sigma;
  const auto g = ofod::greeks::BsmGreeks(OptionType::kCall, 100.0,
                                         {.spot = 100.0,
                                          .rate = r,
                                          .yield = 0.0,
                                          .volatility = sigma,
                                          .maturity = 1.0});
  const double c = p::BsmPrice(OptionType::kCall, 100, 100, r, 0, sigma, 1.0);
  const double option_vol = sigma * 100.0 * g.delta / c;
  std::cout << std::format(
      "Market price of risk {:.4f}: the call has volatility {:.2f}% and so "
      "expected return {:.2f}% = r + lambda sigma_c\n",
      lambda, 100.0 * option_vol, 100.0 * (r + lambda * option_vol));
}

void ThreeNumeraires() {
  const double s0 = 100.0;
  const double k = 100.0;
  const double r = 0.05;
  const double v = 0.2;
  const double t = 1.0;
  const double vt = v * std::sqrt(t);
  const double bsm = p::BsmPrice(OptionType::kCall, s0, k, r, 0.0, v, t);
  // Money market: S_T has drift r; discount the payoff at r.
  const auto bank = Simulate(1, [&](auto& rng) {
    const double s = s0 * std::exp((r - 0.5 * v * v) * t + vt * rng.Next());
    return std::exp(-r * t) * std::max(s - k, 0.0);
  });
  // The stock as numeraire: S_T has drift r + sigma^2; c = S0 E[payoff/S_T].
  const auto stock = Simulate(2, [&](auto& rng) {
    const double s = s0 * std::exp((r + 0.5 * v * v) * t + vt * rng.Next());
    return s0 * std::max(s - k, 0.0) / s;
  });
  // Zero-coupon bond (forward measure): the forward price is a martingale.
  const double forward = s0 * std::exp(r * t);
  const auto bond = Simulate(3, [&](auto& rng) {
    const double f = forward * std::exp(-0.5 * v * v * t + vt * rng.Next());
    return std::exp(-r * t) * std::max(f - k, 0.0);
  });
  std::cout << std::format(
      "1-year ATM call, S = 100, r = 5%, sigma = 20%: BSM {:.4f}\n", bsm);
  std::cout << std::format("  money market numeraire  {:.4f} (se {:.4f})\n",
                           bank.mean, bank.se);
  std::cout << std::format("  stock numeraire         {:.4f} (se {:.4f})\n",
                           stock.mean, stock.se);
  std::cout << std::format("  zero-coupon bond        {:.4f} (se {:.4f})\n",
                           bond.mean, bond.se);
}

void Exchange() {
  const p::ExchangeAssets a{.receive_spot = 100.0,
                            .deliver_spot = 95.0,
                            .receive_yield = 0.02,
                            .deliver_yield = 0.01,
                            .receive_vol = 0.3,
                            .deliver_vol = 0.2,
                            .correlation = 0.4};
  const double r = 0.05;
  const double t = 1.0;
  const double rho_bar = std::sqrt(1.0 - a.correlation * a.correlation);
  // Risk-neutral: simulate both assets.
  const auto both = Simulate(4, [&](auto& rng) {
    const double z1 = rng.Next();
    const double z2 = a.correlation * z1 + rho_bar * rng.Next();
    const double v =
        a.receive_spot *
        std::exp((r - a.receive_yield - 0.5 * a.receive_vol * a.receive_vol) *
                     t +
                 a.receive_vol * std::sqrt(t) * z1);
    const double u =
        a.deliver_spot *
        std::exp((r - a.deliver_yield - 0.5 * a.deliver_vol * a.deliver_vol) *
                     t +
                 a.deliver_vol * std::sqrt(t) * z2);
    return std::exp(-r * t) * std::max(v - u, 0.0);
  });
  // U as numeraire: only the ratio V/U is simulated, and no rate appears.
  const double var = a.receive_vol * a.receive_vol +
                     a.deliver_vol * a.deliver_vol -
                     2.0 * a.correlation * a.receive_vol * a.deliver_vol;
  const auto ratio = Simulate(5, [&](auto& rng) {
    const double x =
        a.receive_spot / a.deliver_spot *
        std::exp((a.deliver_yield - a.receive_yield - 0.5 * var) * t +
                 std::sqrt(var * t) * rng.Next());
    return a.deliver_spot * std::exp(-a.deliver_yield * t) *
           std::max(x - 1.0, 0.0);
  });
  std::cout << std::format(
      "Exchange option (Ch 26.14): Margrabe {:.4f}, risk-neutral MC {:.4f} "
      "(se {:.4f}), U-numeraire MC {:.4f} (se {:.4f})\n",
      p::ExchangeOptionPrice(a, t), both.mean, both.se, ratio.mean, ratio.se);
}

}  // namespace

int main() {
  MarketPriceOfRisk();
  ThreeNumeraires();
  Exchange();
  return 0;
}
