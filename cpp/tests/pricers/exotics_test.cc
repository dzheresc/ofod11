// Copyright 2026 Denis Zhereshchin
//
// Exotic closed forms against Hull's examples, parities and Monte Carlo
// (Ch 26).

#include "ofod/pricers/exotics.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <span>
#include <vector>

#include "ofod/math/special.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/monte_carlo.h"

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

constexpr OptionType kCall = OptionType::kCall;
constexpr OptionType kPut = OptionType::kPut;

McResult Simulate(const TreeMarket& m, double t, int steps,
                  const std::function<double(std::span<const double>)>& f,
                  int paths = 100'000) {
  // The simulator discounts the mean payoff itself.
  const auto r = SimulateGbmPaths(m, t, steps, f,
                                  {.paths = paths, .seed = 11, .threads = 4});
  REQUIRE(r.has_value());
  return *r;
}

TEST_CASE("Hull's exotic examples", "[ch26]") {
  // Example 26.1 gap put, Ch 26.11 lookbacks, Example 26.3 Asians.
  CHECK_THAT(
      GapOptionPrice(kPut, {500'000, 0.05, 0.0, 0.2}, 400'000, 350'000, 1.0),
      WithinAbs(1895.68, 0.01));
  const TreeMarket look{50.0, 0.1, 0.0, 0.4};
  CHECK_THAT(*FloatingLookbackPrice(kCall, look, 50.0, 0.25),
             WithinAbs(8.04, 0.005));
  CHECK_THAT(*FloatingLookbackPrice(kPut, look, 50.0, 0.25),
             WithinAbs(7.79, 0.005));
  const TreeMarket asian{50.0, 0.1, 0.0, 0.4};
  CHECK_THAT(*ArithmeticAsianPrice(kCall, asian, 50.0, 1.0),
             WithinAbs(5.62, 0.005));
  CHECK_THAT(GeometricAsianPrice(kCall, asian, 50.0, 1.0),
             WithinAbs(5.13, 0.005));
}

TEST_CASE("Binary options add up to vanillas", "[ch26]") {
  const TreeMarket m{100.0, 0.05, 0.02, 0.3};
  for (double k : {80.0, 100.0, 130.0}) {
    for (OptionType type : {kCall, kPut}) {
      const double sign = type == kCall ? 1.0 : -1.0;
      CHECK_THAT(
          sign * (AssetOrNothingPrice(type, m, k, 0.7) -
                  CashOrNothingPrice(type, m, k, 0.7, k)),
          WithinAbs(BsmPrice(type, 100.0, k, 0.05, 0.02, 0.3, 0.7), 1e-12));
    }
    CHECK_THAT(CashOrNothingPrice(kCall, m, k, 0.7) +
                   CashOrNothingPrice(kPut, m, k, 0.7),
               WithinAbs(std::exp(-0.05 * 0.7), 1e-14));
  }
  // A gap option with equal strike and trigger is a vanilla.
  CHECK_THAT(
      GapOptionPrice(kCall, m, 90.0, 90.0, 0.7),
      WithinAbs(BsmPrice(kCall, 100.0, 90.0, 0.05, 0.02, 0.3, 0.7), 1e-12));
}

TEST_CASE("Compound options satisfy parity and match Monte Carlo", "[ch26]") {
  const TreeMarket m{50.0, 0.05, 0.01, 0.3};
  const double k1 = 3.0;
  const double t1 = 0.25;
  const double k2 = 50.0;
  const double t2 = 1.0;
  const double pay = k1 * std::exp(-0.05 * t1);
  const double coc =
      *CompoundOptionPrice(CompoundType::kCallOnCall, m, k1, t1, k2, t2);
  const double poc =
      *CompoundOptionPrice(CompoundType::kPutOnCall, m, k1, t1, k2, t2);
  const double cop =
      *CompoundOptionPrice(CompoundType::kCallOnPut, m, k1, t1, k2, t2);
  const double pop =
      *CompoundOptionPrice(CompoundType::kPutOnPut, m, k1, t1, k2, t2);
  CHECK_THAT(
      coc - poc,
      WithinAbs(BsmPrice(kCall, 50.0, k2, 0.05, 0.01, 0.3, t2) - pay, 1e-9));
  CHECK_THAT(
      cop - pop,
      WithinAbs(BsmPrice(kPut, 50.0, k2, 0.05, 0.01, 0.3, t2) - pay, 1e-9));
  // Monte Carlo on S_{t1}: the compound pays max(c(S_{t1}) - k1, 0).
  const auto mc = Simulate(
      m, t1, 1,
      [&](std::span<const double> p) {
        const double c =
            BsmPrice(kCall, p.back(), k2, 0.05, 0.01, 0.3, t2 - t1);
        return std::max(c - k1, 0.0);
      },
      100'000);
  CHECK_THAT(mc.price, WithinAbs(coc, 4.0 * mc.std_error));
  CHECK_FALSE(
      CompoundOptionPrice(CompoundType::kCallOnPut, m, 60.0, t1, k2, t2));
}

TEST_CASE("Chooser, forward start and perpetual options", "[ch26]") {
  const TreeMarket m{50.0, 0.08, 0.0, 0.25};
  // Choosing at expiry is a straddle; choosing now is the dearer vanilla.
  CHECK_THAT(ChooserPrice(m, 50.0, 0.5 - 1e-12, 0.5),
             WithinAbs(BsmPrice(kCall, 50.0, 50.0, 0.08, 0.0, 0.25, 0.5) +
                           BsmPrice(kPut, 50.0, 50.0, 0.08, 0.0, 0.25, 0.5),
                       1e-6));
  CHECK_THAT(ChooserPrice(m, 50.0, 0.25, 0.5), WithinAbs(6.1071, 5e-5));
  // Forward start without dividends is worth the at-the-money option now.
  CHECK_THAT(
      ForwardStartPrice(kCall, m, 0.3, 1.0),
      WithinAbs(BsmPrice(kCall, 50.0, 50.0, 0.08, 0.0, 0.25, 0.7), 1e-12));
  // Perpetual put: value matches smooth pasting at the boundary.
  const TreeMarket p{50.0, 0.05, 0.0, 0.3};
  const double v = *PerpetualAmericanPrice(kPut, p, 50.0);
  CHECK(v > 0.0);
  CHECK(v > BsmPrice(kPut, 50.0, 50.0, 0.05, 0.0, 0.3, 30.0));
  CHECK_FALSE(PerpetualAmericanPrice(kCall, p, 50.0));
  const TreeMarket deep{10.0, 0.05, 0.0, 0.3};
  CHECK_THAT(*PerpetualAmericanPrice(kPut, deep, 50.0), WithinAbs(40.0, 0.0));
}

TEST_CASE("Barrier in plus out is vanilla and matches Monte Carlo", "[ch26]") {
  const TreeMarket m{100.0, 0.08, 0.04, 0.25};
  for (OptionType type : {kCall, kPut}) {
    const double vanilla = BsmPrice(type, 100.0, 100.0, 0.08, 0.04, 0.25, 0.5);
    for (double level : {90.0, 95.0, 105.0, 110.0}) {
      const bool down = level < 100.0;
      const auto out = down ? BarrierType::kDownAndOut : BarrierType::kUpAndOut;
      const auto in = down ? BarrierType::kDownAndIn : BarrierType::kUpAndIn;
      CHECK_THAT(BarrierOptionPrice(out, type, m, 100.0, level, 0.5) +
                     BarrierOptionPrice(in, type, m, 100.0, level, 0.5),
                 WithinAbs(vanilla, 1e-10));
    }
  }
  // Strike below a down barrier, and a breached barrier.
  CHECK_THAT(
      BarrierOptionPrice(BarrierType::kDownAndIn, kCall, m, 90.0, 95.0, 0.5) +
          BarrierOptionPrice(BarrierType::kDownAndOut, kCall, m, 90.0, 95.0,
                             0.5),
      WithinAbs(BsmPrice(kCall, 100.0, 90.0, 0.08, 0.04, 0.25, 0.5), 1e-10));
  CHECK(BarrierOptionPrice(BarrierType::kUpAndOut, kCall, m, 100.0, 99.0,
                           0.5) == 0.0);
  // Discrete monitoring with the Broadie-Glasserman-Kou shift H e^{-0.5826
  // sigma sqrt(dt)} brings Monte Carlo onto the continuous price.
  constexpr int kSteps = 100;
  const double shift = std::exp(0.5826 * 0.25 * std::sqrt(0.5 / kSteps));
  const double level = 95.0;
  const auto mc = Simulate(m, 0.5, kSteps, [&](std::span<const double> p) {
    const bool hit = *std::min_element(p.begin(), p.end()) <= level;
    return hit ? 0.0 : std::max(p.back() - 100.0, 0.0);
  });
  CHECK_THAT(mc.price,
             WithinAbs(BarrierOptionPrice(BarrierType::kDownAndOut, kCall, m,
                                          100.0, level / shift, 0.5),
                       4.0 * mc.std_error));
}

TEST_CASE("Lookbacks and Asians against Monte Carlo", "[ch26]") {
  const TreeMarket m{50.0, 0.1, 0.02, 0.4};
  constexpr int kSteps = 100;
  // Continuous minimum, approximated with the BGK shift on the extreme.
  const double shift = std::exp(0.5826 * 0.4 * std::sqrt(1.0 / kSteps));
  const auto floating = Simulate(m, 1.0, kSteps, [&](auto p) {
    return p.back() - *std::min_element(p.begin(), p.end()) / shift;
  });
  CHECK_THAT(floating.price,
             WithinAbs(*FloatingLookbackPrice(kCall, m, 50.0, 1.0),
                       4.0 * floating.std_error));
  const auto fixed = Simulate(m, 1.0, kSteps, [&](auto p) {
    return std::max(*std::max_element(p.begin(), p.end()) * shift - 55.0, 0.0);
  });
  CHECK_THAT(fixed.price,
             WithinAbs(*FixedLookbackPrice(kCall, m, 55.0, 50.0, 1.0),
                       4.0 * fixed.std_error));
  // Fixed lookback put at expiry is K - min.
  CHECK_THAT(*FixedLookbackPrice(kPut, m, 55.0, 45.0, 1e-10),
             WithinAbs(10.0, 1e-6));
  // Geometric average over many dates converges to the continuous formula.
  const auto geo = Simulate(m, 1.0, kSteps, [](auto p) {
    double log_sum = 0.0;
    for (std::size_t i = 1; i < p.size(); ++i) log_sum += std::log(p[i]);
    return std::max(std::exp(log_sum / (p.size() - 1)) - 50.0, 0.0);
  });
  CHECK_THAT(geo.price, WithinAbs(GeometricAsianPrice(kCall, m, 50.0, 1.0),
                                  4.0 * geo.std_error + 0.02));
  const auto arith = Simulate(m, 1.0, kSteps, [](auto p) {
    double sum = 0.0;
    for (std::size_t i = 1; i < p.size(); ++i) sum += p[i];
    return std::max(sum / (p.size() - 1) - 50.0, 0.0);
  });
  // Moment matching is an approximation: within 2% at 40% volatility.
  CHECK_THAT(arith.price,
             WithinRel(*ArithmeticAsianPrice(kCall, m, 50.0, 1.0), 0.02));
}

TEST_CASE("Exchange option and variance swap", "[ch26]") {
  const ExchangeAssets a{.receive_spot = 100.0,
                         .deliver_spot = 95.0,
                         .receive_yield = 0.02,
                         .deliver_yield = 0.01,
                         .receive_vol = 0.3,
                         .deliver_vol = 0.2,
                         .correlation = 0.4};
  // With a riskless asset to deliver, Margrabe is BSM.
  ExchangeAssets riskless = a;
  riskless.deliver_vol = 0.0;
  riskless.deliver_yield = 0.05;
  CHECK_THAT(
      ExchangeOptionPrice(riskless, 1.0),
      WithinAbs(BsmPrice(kCall, 100.0, 95.0, 0.05, 0.02, 0.3, 1.0), 1e-12));
  CHECK(ExchangeOptionPrice(a, 1.0) > 0.0);
  // Flat volatility: the fair variance is sigma^2, up to the strike grid.
  std::vector<double> strikes;
  for (double k = 20.0; k <= 400.0; k += 0.5) strikes.push_back(k);
  const TreeMarket m{100.0, 0.04, 0.01, 0.25};
  const auto var =
      VarianceSwapFairVariance(m, 0.5, strikes, [](double) { return 0.25; });
  REQUIRE(var.has_value());
  CHECK_THAT(*var, WithinAbs(0.0625, 2e-5));
  CHECK_FALSE(VarianceSwapFairVariance(m, 0.5, std::vector<double>{1, 2, 3},
                                       [](double) { return 0.25; }));
}

TEST_CASE("Incomplete gamma and noncentral chi-square", "[math][ch27]") {
  CHECK_THAT(math::RegularizedGammaP(1.0, 2.0),
             WithinAbs(1.0 - std::exp(-2.0), 1e-15));
  CHECK_THAT(math::RegularizedGammaP(0.5, 0.5),
             WithinAbs(std::erf(std::sqrt(0.5)), 1e-14));
  CHECK_THAT(math::RegularizedGammaP(30.0, 50.0),
             WithinAbs(0.999083171138544, 1e-13));
  // Two degrees of freedom, no noncentrality: 1 - e^{-x/2}.
  CHECK_THAT(math::NoncentralChiSquareCdf(3.0, 2.0, 0.0),
             WithinAbs(1.0 - std::exp(-1.5), 1e-15));
  // scipy.stats.ncx2.cdf(5, 3, 2) and scipy.special.gammainc(30, 50).
  CHECK_THAT(math::NoncentralChiSquareCdf(5.0, 3.0, 2.0),
             WithinAbs(0.5934051800831555, 1e-13));
  CHECK(std::isnan(math::RegularizedGammaP(-1.0, 1.0)));
}

}  // namespace
}  // namespace ofod::pricers
