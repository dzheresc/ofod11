// Copyright 2026 Denis Zhereshchin
//
// Black's model for bond options, caps and swaptions, the normal model and
// the Ch 30 adjustments (Hull, Ch 29, 30, 34.4).

#include "ofod/pricers/rate_options.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/pricers/adjustments.h"
#include "ofod/pricers/exotic_swaps.h"
#include "ofod/random/normal_generator.h"

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

constexpr OptionType kCall = OptionType::kCall;
constexpr OptionType kPut = OptionType::kPut;

curves::ZeroCurve Upward() {
  return *curves::ZeroCurve::Create({0.25, 1.0, 2.0, 5.0, 10.0},
                                    {0.03, 0.035, 0.04, 0.045, 0.05});
}

TEST_CASE("Black's model reproduces Hull's bond option example", "[ch29]") {
  // Example 29.1: 10-month call on a 9.75-year bond, cash price 960.
  const double coupons =
      50.0 * std::exp(-0.25 * 0.09) + 50.0 * std::exp(-0.75 * 0.095);
  const double discount = std::exp(-0.1 * 10.0 / 12.0);
  const double forward = ForwardBondPrice(960.0, coupons, discount);
  CHECK_THAT(forward, WithinAbs(939.68, 0.005));
  CHECK_THAT(
      BlackBondOptionPrice(kCall, forward, 1000.0, discount, 0.09, 10.0 / 12),
      WithinAbs(9.49, 0.005));
  // Put-call parity on the forward.
  CHECK_THAT(
      BlackBondOptionPrice(kCall, forward, 1000.0, discount, 0.09, 0.8) -
          BlackBondOptionPrice(kPut, forward, 1000.0, discount, 0.09, 0.8),
      WithinAbs(discount * (forward - 1000.0), 1e-9));
  CHECK_THAT(PriceVolatilityFromYield(5.2, 0.08, 0.2),
             WithinAbs(0.0832, 1e-12));
}

TEST_CASE("Caplets, caps and floors under Black", "[ch29]") {
  // Example 29.2 inputs: L = 10,000, R_K = 8%, F = 7%, 20% vol.
  CHECK_THAT(BlackCapletPrice(kCall, 0.07, 0.08, 0.2, 1.0, 0.25,
                              std::exp(-0.065 * 1.25), 10'000.0),
             WithinAbs(5.19, 0.005));
  const curves::ZeroCurve curve = Upward();
  const CapTerms terms{.strike = 0.04, .maturity = 5.0, .frequency = 4};
  CHECK(CapletCount(terms) == 19);
  CHECK(CapletCount({.strike = 0.04, .maturity = 0.25}) == 0);
  // Cap - floor = swap paying the floating rate against the strike.
  const double cap = *CapFloorPrice(kCall, curve, terms, 0.2);
  const double floor = *CapFloorPrice(kPut, curve, terms, 0.2);
  double swap = 0.0;
  for (int k = 1; k < 20; ++k) {
    const double reset = 0.25 * k;
    swap += 0.25 * (SimpleForwardRate(curve, reset, reset + 0.25) - 0.04) *
            curve.DiscountFactor(reset + 0.25);
  }
  CHECK_THAT(cap - floor, WithinAbs(swap, 1e-12));
  CHECK_FALSE(CapletPrices(kCall, curve, terms, std::vector<double>{0.2}));
}

TEST_CASE("Caplet volatility stripping inverts flat cap vols", "[ch29]") {
  const curves::ZeroCurve curve = Upward();
  // A flat term structure strips to the same spot vols.
  const std::vector<double> flat(7, 0.2);
  const auto same = StripCapletVolatilities(curve, 0.04, 2, flat);
  REQUIRE(same.has_value());
  for (double v : *same) CHECK_THAT(v, WithinAbs(0.2, 1e-9));
  // Humped flat vols: the stripped vols reprice every cap.
  const std::vector<double> humped = {0.18, 0.2, 0.21, 0.21, 0.2, 0.19};
  const auto spot = StripCapletVolatilities(curve, 0.04, 2, humped);
  REQUIRE(spot.has_value());
  for (std::size_t i = 0; i < humped.size(); ++i) {
    const CapTerms terms{.strike = 0.04,
                         .maturity = 0.5 * static_cast<double>(i + 2),
                         .frequency = 2};
    const auto caplets = CapletPrices(
        kCall, curve, terms, std::span<const double>(*spot).first(i + 1));
    double total = 0.0;
    for (double c : *caplets) total += c;
    CHECK_THAT(total, WithinAbs(*CapFloorPrice(kCall, curve, terms, humped[i]),
                                1e-10));
  }
  // Spot vols move more than flat vols.
  CHECK((*spot)[2] > humped[2]);
}

TEST_CASE("Black swaptions reproduce Hull's example", "[ch29]") {
  // Example 29.3: pay 6.2% in a 3-year swap starting in 5 years.
  const curves::ZeroCurve flat = curves::ZeroCurve::Flat(0.06);
  const ForwardSwap swap = ForwardSwapRate(flat, 5.0, 3.0, 2);
  CHECK_THAT(swap.annuity, WithinAbs(2.0035, 1e-3));
  CHECK_THAT(swap.rate, WithinAbs(0.06091, 1e-5));
  CHECK_THAT(BlackSwaptionPrice(kCall, swap.rate, 0.062, 0.2, 5.0, swap.annuity,
                                100.0),
             WithinAbs(2.07, 0.005));
  // Payer - receiver = forward swap.
  CHECK_THAT(
      BlackSwaptionPrice(kCall, swap.rate, 0.062, 0.2, 5.0, swap.annuity, 1.0) -
          BlackSwaptionPrice(kPut, swap.rate, 0.062, 0.2, 5.0, swap.annuity,
                             1.0),
      WithinAbs(swap.annuity * (swap.rate - 0.062), 1e-14));
}

TEST_CASE("The normal model handles negative rates", "[ch29]") {
  // Parity holds for any forward; at the money the price is sd / sqrt(2pi).
  CHECK_THAT(BachelierFormula(kCall, -0.002, 0.001, 0.006, 2.0) -
                 BachelierFormula(kPut, -0.002, 0.001, 0.006, 2.0),
             WithinAbs(-0.003, 1e-15));
  CHECK_THAT(BachelierFormula(kCall, 0.01, 0.01, 0.006, 2.0),
             WithinAbs(0.006 * std::sqrt(2.0) / std::sqrt(2.0 * M_PI), 1e-15));
  // Matches Black near the money for small vols: sigma_N = sigma F.
  CHECK_THAT(
      BachelierSwaptionPrice(kCall, 0.05, 0.05, 0.05 * 0.1, 1.0, 4.0, 1.0),
      WithinRel(BlackSwaptionPrice(kCall, 0.05, 0.05, 0.1, 1.0, 4.0, 1.0),
                1e-3));
}

TEST_CASE("Bucketed DV01s add up to the parallel DV01", "[ch29]") {
  const curves::ZeroCurve curve = Upward();
  const CurvePricer cap = [](const curves::ZeroCurve& c) {
    return *CapFloorPrice(kCall, c, {.strike = 0.04, .maturity = 5.0}, 0.2);
  };
  const std::vector<double> buckets = BucketDv01(cap, curve);
  double sum = 0.0;
  for (double b : buckets) sum += b;
  CHECK(buckets.size() == 5);
  CHECK(buckets.back() == 0.0);  // The 10-year pillar does not move a 5y cap.
  CHECK_THAT(sum, WithinRel(ParallelDv01(cap, curve), 1e-2));
}

TEST_CASE("Convexity and timing adjustments reproduce Hull", "[ch30]") {
  // Example 30.1: payoff in 3 years of the 1-year rate, 10% annual, 20%.
  const double adj = BondYieldConvexityAdjustment(0.1, 0.2, 3.0, 0.0, 1.0, 1);
  CHECK_THAT(adj, WithinAbs(0.00109, 1e-5));
  CHECK_THAT(1000.0 * (0.1 + adj) / std::pow(1.1, 3), WithinAbs(75.95, 0.005));
  // Example 30.3: the same payoff made in 4 years.
  const double rate =
      (0.1 + adj) * TimingAdjustmentFactor(0.1, 0.2, 0.2, 1.0, 3.0, 1.0);
  CHECK_THAT(1000.0 * rate / std::pow(1.1, 4), WithinAbs(68.30, 0.005));
  // The direct formula with numerical G derivatives agrees.
  auto g = [](double y) { return 1.0 / (1.0 + y); };
  const double h = 1e-4;
  const double g1 = (g(0.1 + h) - g(0.1 - h)) / (2 * h);
  const double g2 = (g(0.1 + h) - 2 * g(0.1) + g(0.1 - h)) / (h * h);
  CHECK_THAT(ConvexityAdjustment(0.1, 0.2, 3.0, g1, g2), WithinAbs(adj, 1e-8));
  CHECK_THAT(QuantoAdjustmentFactor(-0.3, 0.12, 0.25, 2.0),
             WithinAbs(std::exp(-0.018), 1e-15));
}

TEST_CASE("In-arrears rate matches Monte Carlo under the T-forward measure",
          "[ch34]") {
  // Under the t + tau forward measure F is a driftless lognormal. A
  // payment F_t at t is worth F_t (1 + F_t tau) at t + tau, so its value
  // per unit of P(0, t) is E[F (1 + F tau)] / (1 + F0 tau).
  const double f0 = 0.05;
  const double vol = 0.25;
  const double t = 3.0;
  const double tau = 0.5;
  random::NormalGenerator rng(11);
  double sum = 0.0;
  constexpr int kPaths = 400'000;
  for (int i = 0; i < kPaths; ++i) {
    const double f =
        f0 * std::exp(-0.5 * vol * vol * t + vol * std::sqrt(t) * rng.Next());
    sum += f * (1.0 + f * tau);
  }
  const double expected = sum / kPaths / (1.0 + f0 * tau);
  CHECK_THAT(InArrearsRate(f0, vol, t, tau), WithinRel(expected, 2e-3));
}

TEST_CASE("Nonstandard swaps", "[ch34]") {
  const curves::ZeroCurve curve = Upward();
  auto periods = SwapSchedule(1.0, 6.0, 2, 100.0, 0.0, 8.0, 0.0);
  CHECK(periods.size() == 10);
  CHECK(periods.back().notional == 28.0);
  const double par = ParFixedRate(curve, periods);
  for (auto& p : periods) p.fixed_rate = par;
  CHECK_THAT(ScheduleSwapValue(curve, periods), WithinAbs(0.0, 1e-12));
  // A step-up schedule with a zero step is the plain swap.
  const auto plain = SwapSchedule(0.0, 3.0, 4, 1.0, 0.05);
  const auto stepped = SwapSchedule(0.0, 3.0, 4, 1.0, 0.05, 0.0, 0.0);
  CHECK(ScheduleSwapValue(curve, plain) == ScheduleSwapValue(curve, stepped));
  // In arrears with zero vol is a swap on the next period's forwards.
  const std::vector<double> zero(12, 0.0);
  const std::vector<double> vols(12, 0.2);
  double next = 0.0;
  for (const auto& p : plain) {
    next += 0.25 * (0.05 - SimpleForwardRate(curve, p.end, p.end + 0.25)) *
            curve.DiscountFactor(p.end);
  }
  CHECK_THAT(InArrearsSwapValue(curve, plain, zero), WithinAbs(next, 1e-14));
  CHECK(InArrearsSwapValue(curve, plain, vols) < next);
  // A CMS leg with zero vols pays the forward swap rates.
  const CmsLeg leg{.cms_tenor = 5.0, .cms_frequency = 2, .forward_vol = 0.0};
  double cms = 0.0;
  for (const auto& p : plain) {
    cms += 0.25 * (0.05 - ForwardSwapRate(curve, p.start, 5.0, 2).rate) *
           curve.DiscountFactor(p.end);
  }
  CHECK_THAT(CmsSwapValue(curve, plain, leg, zero), WithinAbs(cms, 1e-14));
  CHECK(CmsSwapValue(curve, plain,
                     {.cms_tenor = 5.0, .cms_frequency = 2, .forward_vol = 0.2},
                     vols) < cms);
  // An accrual swap with an unbounded range is a plain fixed leg.
  double fixed = 0.0;
  for (const auto& p : plain)
    fixed += 0.25 * 0.05 * curve.DiscountFactor(p.end);
  CHECK_THAT(AccrualFixedLegValue(curve, plain, 0.0, 1e9, 0.2, 30),
             WithinAbs(fixed, 1e-12));
  CHECK(AccrualFixedLegValue(curve, plain, 0.03, 0.05, 0.2, 30) < 0.8 * fixed);
}

}  // namespace
}  // namespace ofod::pricers
