// Copyright 2026 Denis Zhereshchin
//
// Chapter 26: gap, forward start, chooser, compound, barrier, binary,
// lookback, Asian, exchange and perpetual options, and a variance swap.

#include "ofod/pricers/exotics.h"

#include <array>
#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "ofod/pricers/black_scholes.h"

namespace {

namespace p = ofod::pricers;
using p::OptionType;
using p::TreeMarket;

double Or(std::optional<double> v) { return v.value_or(0.0); }

void Simple() {
  std::cout << std::format(
      "Gap put (Example 26.1): {:.2f}\n",
      p::GapOptionPrice(OptionType::kPut, {500'000, 0.05, 0.0, 0.2}, 400'000,
                        350'000, 1.0));
  const TreeMarket m{50.0, 0.08, 0.0, 0.25};
  std::cout << std::format(
      "Chooser, S = K = 50, choose at 3 months, expiry 6 months: {:.4f} "
      "(call {:.4f} + put {:.4f} bought separately)\n",
      p::ChooserPrice(m, 50.0, 0.25, 0.5),
      p::BsmPrice(OptionType::kCall, 50, 50, 0.08, 0, 0.25, 0.5),
      p::BsmPrice(OptionType::kPut, 50, 50, 0.08, 0, 0.25, 0.5));
  std::cout << std::format(
      "Forward start call, starts in 6 months, 1-year expiry, q = 2%: {:.4f}\n",
      p::ForwardStartPrice(OptionType::kCall, {60.0, 0.05, 0.02, 0.3}, 0.5,
                           1.0));
  const TreeMarket perp{50.0, 0.05, 0.03, 0.3};
  std::cout << std::format(
      "Perpetual American, S = K = 50, q = 3%: call {:.4f}, put {:.4f}\n",
      Or(p::PerpetualAmericanPrice(OptionType::kCall, perp, 50.0)),
      Or(p::PerpetualAmericanPrice(OptionType::kPut, perp, 50.0)));
  const TreeMarket b{100.0, 0.05, 0.02, 0.3};
  std::cout << std::format(
      "Binary call, K = 110, 6 months: cash-or-nothing {:.4f}, "
      "asset-or-nothing {:.4f}\n",
      p::CashOrNothingPrice(OptionType::kCall, b, 110.0, 0.5),
      p::AssetOrNothingPrice(OptionType::kCall, b, 110.0, 0.5));
}

void Compound() {
  const TreeMarket m{50.0, 0.05, 0.0, 0.3};
  std::cout << "Compound options, K1 = 3 at 3 months on K2 = 50 at 1 year\n";
  const std::array<std::pair<const char*, p::CompoundType>, 4> types = {{
      {"call on call", p::CompoundType::kCallOnCall},
      {"put on call", p::CompoundType::kPutOnCall},
      {"call on put", p::CompoundType::kCallOnPut},
      {"put on put", p::CompoundType::kPutOnPut},
  }};
  for (const auto& [name, type] : types) {
    std::cout << std::format(
        "  {:<12} {:.4f}\n", name,
        Or(p::CompoundOptionPrice(type, m, 3.0, 0.25, 50.0, 1.0)));
  }
}

void Barriers() {
  const TreeMarket m{100.0, 0.08, 0.04, 0.25};
  std::cout << "Barrier options, S = K = 100, 6 months (H = 95 down, 105 up)\n";
  std::cout << "  type           call      put\n";
  const std::array<std::pair<const char*, p::BarrierType>, 4> types = {{
      {"down-and-out", p::BarrierType::kDownAndOut},
      {"down-and-in", p::BarrierType::kDownAndIn},
      {"up-and-out", p::BarrierType::kUpAndOut},
      {"up-and-in", p::BarrierType::kUpAndIn},
  }};
  for (const auto& [name, type] : types) {
    const bool down = type == p::BarrierType::kDownAndOut ||
                      type == p::BarrierType::kDownAndIn;
    const double h = down ? 95.0 : 105.0;
    std::cout << std::format(
        "  {:<12} {:8.4f} {:8.4f}\n", name,
        p::BarrierOptionPrice(type, OptionType::kCall, m, 100.0, h, 0.5),
        p::BarrierOptionPrice(type, OptionType::kPut, m, 100.0, h, 0.5));
  }
}

void PathDependent() {
  const TreeMarket look{50.0, 0.1, 0.0, 0.4};
  std::cout << std::format(
      "Lookbacks, S0 = 50, sigma = 40%, r = 10%, 3 months: floating call "
      "{:.4f}, floating put {:.4f}, fixed call K = 50 {:.4f}\n",
      Or(p::FloatingLookbackPrice(OptionType::kCall, look, 50.0, 0.25)),
      Or(p::FloatingLookbackPrice(OptionType::kPut, look, 50.0, 0.25)),
      Or(p::FixedLookbackPrice(OptionType::kCall, look, 50.0, 50.0, 0.25)));
  std::cout << std::format(
      "Asians (Example 26.3), S0 = K = 50, 1 year: geometric {:.4f}, "
      "arithmetic by moment matching {:.4f}, vanilla {:.4f}\n",
      p::GeometricAsianPrice(OptionType::kCall, look, 50.0, 1.0),
      Or(p::ArithmeticAsianPrice(OptionType::kCall, look, 50.0, 1.0)),
      p::BsmPrice(OptionType::kCall, 50, 50, 0.1, 0, 0.4, 1.0));
}

void ExchangeAndVariance() {
  const p::ExchangeAssets a{.receive_spot = 100.0,
                            .deliver_spot = 95.0,
                            .receive_yield = 0.02,
                            .deliver_yield = 0.01,
                            .receive_vol = 0.3,
                            .deliver_vol = 0.2,
                            .correlation = 0.4};
  std::cout << std::format("Exchange V for U in 1 year (Margrabe): {:.4f}\n",
                           p::ExchangeOptionPrice(a, 1.0));
  std::vector<double> strikes;
  for (int k = 50; k <= 200; ++k) strikes.push_back(k);
  const TreeMarket m{100.0, 0.04, 0.01, 0.0};
  const auto flat =
      p::VarianceSwapFairVariance(m, 0.5, strikes, [](double) { return 0.25; });
  const auto skew = p::VarianceSwapFairVariance(m, 0.5, strikes, [](double k) {
    return 0.25 - 0.1 * std::log(k / 100.0);
  });
  std::cout << std::format(
      "Variance swap, 6 months, strikes 50-200: flat 25% vol gives "
      "{:.6f} ({:.2f}% vol); skewed smile gives {:.6f} ({:.2f}% vol)\n",
      Or(flat), 100.0 * std::sqrt(Or(flat)), Or(skew),
      100.0 * std::sqrt(Or(skew)));
}

}  // namespace

int main() {
  try {
    Simple();
    Compound();
    Barriers();
    PathDependent();
    ExchangeAndVariance();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
