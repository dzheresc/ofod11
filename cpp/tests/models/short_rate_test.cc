// Copyright 2026 Denis Zhereshchin
//
// Vasicek, CIR, Ho-Lee and Hull-White, the trinomial trees and their
// calibration (Hull, Ch 31, 32).

#include "ofod/models/short_rate.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/pricers/rate_options.h"
#include "ofod/pricers/short_rate_tree.h"
#include "ofod/random/normal_generator.h"

namespace ofod::models {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using instruments::Exercise;
using pricers::ShortRateModel;
using pricers::ShortRateTree;
using pricers::TreeCashFlow;

constexpr OptionType kCall = OptionType::kCall;
constexpr OptionType kPut = OptionType::kPut;

// Hull's zero curve for the tree examples of Ch 32.
curves::ZeroCurve HullCurve() {
  return *curves::ZeroCurve::Create(
      {0.5, 1.0, 1.5, 2.0, 2.5, 3.0},
      {0.0343, 0.03824, 0.04183, 0.04512, 0.04812, 0.05086});
}

TEST_CASE("Vasicek and CIR bond prices", "[ch31]") {
  const Vasicek v{.a = 0.1, .b = 0.1, .sigma = 0.02};
  // Closed form against a fine Monte Carlo of exp(-integral r).
  random::NormalGenerator rng(3);
  constexpr int kPaths = 20'000;
  constexpr int kSteps = 200;
  const double dt = 5.0 / kSteps;
  double sum = 0.0;
  for (int p = 0; p < kPaths; ++p) {
    double r = 0.05;
    double integral = 0.0;
    for (int i = 0; i < kSteps; ++i) {
      const double next =
          r * std::exp(-v.a * dt) + v.b * (1.0 - std::exp(-v.a * dt)) +
          v.sigma * std::sqrt((1.0 - std::exp(-2 * v.a * dt)) / (2 * v.a)) *
              rng.Next();
      integral += 0.5 * (r + next) * dt;
      r = next;
    }
    sum += std::exp(-integral);
  }
  CHECK_THAT(BondPrice(v, 0.05, 5.0), WithinRel(sum / kPaths, 1e-3));
  // Long-maturity yield tends to b - sigma^2 / (2 a^2).
  CHECK_THAT(ZeroRate(v, 0.05, 2000.0), WithinAbs(0.1 - 0.02, 2e-4));

  const Cir c{.a = 0.2, .b = 0.05, .sigma = 0.1};
  sum = 0.0;
  for (int p = 0; p < 4000; ++p) {
    const auto path = SimulateCir(c, 0.04, 0.05, 100, 100 + p);
    double integral = 0.0;
    for (std::size_t i = 1; i < path.size(); ++i) {
      integral += 0.5 * (path[i - 1] + path[i]) * 0.05;
    }
    sum += std::exp(-integral);
  }
  CHECK_THAT(BondPrice(c, 0.04, 5.0), WithinRel(sum / 4000, 2e-3));
  // Exact CIR transitions have the right mean.
  double mean = 0.0;
  for (int p = 0; p < 4000; ++p) mean += SimulateCir(c, 0.04, 2.0, 1, p)[1];
  CHECK_THAT(mean / 4000,
             WithinAbs(0.05 + (0.04 - 0.05) * std::exp(-0.4), 5e-4));
}

TEST_CASE("Vasicek bond options and estimation", "[ch31]") {
  const Vasicek v{.a = 0.1, .b = 0.1, .sigma = 0.02};
  const double call = ZeroBondOptionPrice(v, kCall, 0.1, 1.0, 5.0, 0.7);
  const double put = ZeroBondOptionPrice(v, kPut, 0.1, 1.0, 5.0, 0.7);
  CHECK_THAT(
      call - put,
      WithinAbs(BondPrice(v, 0.1, 5.0) - 0.7 * BondPrice(v, 0.1, 1.0), 1e-14));
  // Exact simulation of a long daily series recovers the parameters.
  const Vasicek truth{.a = 0.5, .b = 0.04, .sigma = 0.01};
  const double dt = 1.0 / 252;
  random::NormalGenerator rng(9);
  std::vector<double> r = {0.03};
  const double phi = std::exp(-truth.a * dt);
  const double sd =
      truth.sigma * std::sqrt((1.0 - phi * phi) / (2.0 * truth.a));
  for (int i = 0; i < 252 * 200; ++i) {
    r.push_back(truth.b + (r.back() - truth.b) * phi + sd * rng.Next());
  }
  const auto fit = EstimateVasicek(r, dt);
  REQUIRE(fit.has_value());
  CHECK_THAT(fit->a, WithinAbs(0.5, 0.25));  // Standard error ~0.07.
  CHECK_THAT(fit->b, WithinAbs(0.04, 2e-3));
  CHECK_THAT(fit->sigma, WithinRel(0.01, 1e-2));
  CHECK_FALSE(EstimateVasicek(std::vector<double>{0.01, 0.02}, dt));
}

TEST_CASE("Hull-White fits the curve and prices bond options", "[ch32]") {
  const curves::ZeroCurve curve = HullCurve();
  const HullWhite hw = *HullWhite::Create(curve, 0.1, 0.01);
  CHECK_THAT(hw.BondPrice(0.0, 2.5, hw.InstantaneousForward(0.0)),
             WithinAbs(curve.DiscountFactor(2.5), 1e-9));
  const double call = hw.ZeroBondOption(kCall, 1.0, 3.0, 0.92);
  const double put = hw.ZeroBondOption(kPut, 1.0, 3.0, 0.92);
  CHECK_THAT(call - put, WithinAbs(curve.DiscountFactor(3.0) -
                                       0.92 * curve.DiscountFactor(1.0),
                                   1e-14));
  // Ho-Lee is the a -> 0 limit.
  const HullWhite ho_lee = *HullWhite::Create(curve, 0.0, 0.01);
  const HullWhite tiny = *HullWhite::Create(curve, 1e-7, 0.01);
  CHECK_THAT(ho_lee.ZeroBondOption(kPut, 1.0, 3.0, 0.92),
             WithinAbs(tiny.ZeroBondOption(kPut, 1.0, 3.0, 0.92), 1e-8));
  CHECK_FALSE(HullWhite::Create(curve, 0.1, 0.0));
  // Jamshidian: a 1-year option on a 3-year 5% annual coupon bond.
  const std::vector<double> times = {2.0, 3.0};
  const std::vector<double> amounts = {5.0, 105.0};
  const auto jamshidian = hw.CouponBondOption(kCall, 1.0, times, amounts, 99.0);
  REQUIRE(jamshidian.has_value());
  const ShortRateTree tree = *ShortRateTree::Build(curve, 0.1, 0.01, 0.01, 300);
  const std::vector<TreeCashFlow> bond = {{200, 5.0}, {300, 105.0}};
  CHECK_THAT(*pricers::TreeBondOption(tree, kCall, Exercise::kEuropean, 100,
                                      bond, 99.0),
             WithinAbs(*jamshidian, 2e-3));
  // Caplets as bond puts satisfy cap-floor parity.
  const double caplet = hw.Caplet(kCall, 1.0, 0.5, 0.04, 1.0);
  const double floorlet = hw.Caplet(kPut, 1.0, 0.5, 0.04, 1.0);
  CHECK_THAT(
      caplet - floorlet,
      WithinAbs(0.5 * (pricers::SimpleForwardRate(curve, 1.0, 1.5) - 0.04) *
                    curve.DiscountFactor(1.5),
                1e-12));
}

TEST_CASE("Hull-White calibration recovers known parameters", "[ch32]") {
  const curves::ZeroCurve curve = HullCurve();
  const HullWhite truth = *HullWhite::Create(curve, 0.08, 0.012);
  std::vector<CapletQuote> quotes;
  for (double reset : {0.5, 1.0, 1.5, 2.0, 2.5}) {
    for (double strike : {0.04, 0.055}) {
      quotes.push_back(
          {reset, 0.5, strike, truth.Caplet(kCall, reset, 0.5, strike, 1.0)});
    }
  }
  const auto fit = CalibrateHullWhite(curve, quotes);
  REQUIRE(fit.has_value());
  CHECK_THAT(fit->a, WithinAbs(0.08, 1e-4));
  CHECK_THAT(fit->sigma, WithinAbs(0.012, 1e-6));
  CHECK(fit->rms_relative_error < 1e-6);
}

TEST_CASE("Hull-White trinomial tree matches Hull's construction", "[ch32]") {
  const curves::ZeroCurve curve = HullCurve();
  const ShortRateTree tree = *ShortRateTree::Build(curve, 0.1, 0.01, 1.0, 3);
  CHECK(tree.jmax() == 2);
  CHECK_THAT(tree.dx(), WithinAbs(0.01732, 1e-5));
  // Hull's alphas: 3.824%, 5.205%, 6.252%.
  CHECK_THAT(tree.alphas()[0], WithinAbs(0.03824, 1e-5));
  CHECK_THAT(tree.alphas()[1], WithinAbs(0.05205, 1e-5));
  CHECK_THAT(tree.alphas()[2], WithinAbs(0.06252, 1e-5));
  const auto& top = tree.BranchAt(2);
  CHECK(top.middle == 1);
  CHECK_THAT(top.up, WithinAbs(0.8867, 1e-4));
  CHECK_THAT(top.mid, WithinAbs(0.0267, 1e-4));
  CHECK_THAT(top.down, WithinAbs(0.0867, 1e-4));
  CHECK_THAT(tree.BranchAt(1).up, WithinAbs(0.1217, 1e-4));
  // Arrow-Debreu prices reprice the curve at each step.
  for (int i = 0; i <= 3; ++i) {
    double sum = 0.0;
    for (double q : tree.ArrowDebreu(i)) sum += q;
    CHECK_THAT(sum, WithinAbs(curve.DiscountFactor(i * 1.0), 1e-12));
  }
  CHECK_THAT(tree.ZeroBondValues(0, 3)[0],
             WithinAbs(curve.DiscountFactor(3.0), 1e-12));
  CHECK_FALSE(ShortRateTree::Build(curve, 0.1, 0.01, 0.0, 3));
}

TEST_CASE("Black-Karasinski tree fits the curve", "[ch32]") {
  const curves::ZeroCurve curve = HullCurve();
  const auto tree = ShortRateTree::Build(curve, 0.22, 0.25, 0.25, 12,
                                         ShortRateModel::kBlackKarasinski);
  REQUIRE(tree.has_value());
  for (int i = 1; i <= 12; ++i) {
    CHECK_THAT(tree->ZeroBondValues(0, i)[0],
               WithinAbs(curve.DiscountFactor(0.25 * i), 1e-12));
  }
  for (int j = -tree->Width(11); j <= tree->Width(11); ++j) {
    CHECK(tree->Rate(11, j) > 0.0);
  }
}

TEST_CASE("Tree swaptions and American bond options", "[ch32]") {
  const curves::ZeroCurve curve = HullCurve();
  const HullWhite hw = *HullWhite::Create(curve, 0.1, 0.01);
  const ShortRateTree tree = *ShortRateTree::Build(curve, 0.1, 0.01, 0.02, 150);
  // European receiver swaption at 1 year into a 2-year annual swap is a
  // call on a coupon bond with strike 1 (Jamshidian).
  const std::vector<int> once = {50};
  const double strike = 0.05;
  const auto tree_euro =
      pricers::TreeSwaption(tree, kPut, strike, 50, 150, once);
  const auto analytic =
      hw.CouponBondOption(kCall, 1.0, std::vector<double>{2.0, 3.0},
                          std::vector<double>{strike, 1.0 + strike}, 1.0);
  REQUIRE(tree_euro.has_value());
  CHECK_THAT(*tree_euro, WithinAbs(*analytic, 2e-5));
  // Payer - receiver = forward swap.
  const double payer =
      *pricers::TreeSwaption(tree, kCall, strike, 50, 150, once);
  const double forward_swap =
      curve.DiscountFactor(1.0) - curve.DiscountFactor(3.0) -
      strike * (curve.DiscountFactor(2.0) + curve.DiscountFactor(3.0));
  CHECK_THAT(payer - *tree_euro, WithinAbs(forward_swap, 1e-10));
  // Bermudan >= European.
  const std::vector<int> both = {50, 100};
  CHECK(*pricers::TreeSwaption(tree, kPut, strike, 50, 150, both) > *tree_euro);
  CHECK_FALSE(
      pricers::TreeSwaption(tree, kPut, strike, 50, 150, std::vector<int>{60}));
  // American put on a zero-coupon bond >= European.
  const std::vector<TreeCashFlow> zero = {{150, 100.0}};
  const double euro = *pricers::TreeBondOption(tree, kPut, Exercise::kEuropean,
                                               100, zero, 90.0);
  const double amer = *pricers::TreeBondOption(tree, kPut, Exercise::kAmerican,
                                               100, zero, 90.0);
  CHECK(amer >= euro);
  CHECK_THAT(euro,
             WithinAbs(100.0 * hw.ZeroBondOption(kPut, 2.0, 3.0, 0.9), 2e-3));
}

}  // namespace
}  // namespace ofod::models
