// Copyright 2026 Denis Zhereshchin
//
// CEV, jump diffusion, Heston, Longstaff-Schwartz and convertibles
// (Hull, Ch 27).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

#include "ofod/math/quadrature.h"
#include "ofod/pricers/american_mc.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/model_pricers.h"
#include "ofod/random/normal_generator.h"

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using instruments::Exercise;
using instruments::VanillaOption;

constexpr OptionType kCall = OptionType::kCall;
constexpr OptionType kPut = OptionType::kPut;

TEST_CASE("CEV matches scipy's noncentral chi-square", "[ch27]") {
  // References from scipy.stats.ncx2 with sigma = 0.2 * 100^(1 - alpha).
  const TreeMarket base{100.0, 0.05, 0.01, 0.0};
  struct Case {
    double alpha;
    double strike;
    double expected;
  };
  for (const Case& c : {Case{0.3, 80.0, 23.91831509903416},
                        Case{0.7, 120.0, 2.7950371581577684},
                        Case{1.5, 100.0, 9.829549345139938},
                        Case{2.0, 120.0, 3.618806013532918}}) {
    TreeMarket m = base;
    m.volatility = 0.2 * std::pow(100.0, 1.0 - c.alpha);
    CHECK_THAT(*CevPrice(kCall, m, c.strike, 1.0, c.alpha),
               WithinAbs(c.expected, 1e-8));
    // Put-call parity.
    CHECK_THAT(
        *CevPrice(kCall, m, c.strike, 1.0, c.alpha) -
            *CevPrice(kPut, m, c.strike, 1.0, c.alpha),
        WithinAbs(100.0 * std::exp(-0.01) - c.strike * std::exp(-0.05), 1e-10));
  }
  CHECK_FALSE(CevPrice(kCall, base, 100.0, 1.0, 1.0));
}

TEST_CASE("Merton jump diffusion reduces to BSM and matches Monte Carlo",
          "[ch27]") {
  const TreeMarket m{100.0, 0.05, 0.01, 0.2};
  CHECK_THAT(
      MertonJumpPrice(kCall, m, 105.0, 1.0, {0.0, 0.0, 0.1}),
      WithinAbs(BsmPrice(kCall, 100.0, 105.0, 0.05, 0.01, 0.2, 1.0), 1e-12));
  const JumpParams jumps{.intensity = 1.0, .mean_jump = -0.1, .jump_vol = 0.2};
  const double analytic = MertonJumpPrice(kCall, m, 105.0, 1.0, jumps);
  // Exact simulation of S_T: Poisson number of lognormal jumps.
  random::NormalGenerator rng(5);
  std::mt19937_64 poisson_rng(6);
  std::poisson_distribution<int> poisson(jumps.intensity);
  const double drift =
      0.05 - 0.01 - jumps.intensity * jumps.mean_jump - 0.5 * 0.2 * 0.2;
  const double jump_mean =
      std::log1p(jumps.mean_jump) - 0.5 * jumps.jump_vol * jumps.jump_vol;
  constexpr int kPaths = 200'000;
  double sum = 0.0;
  double sum_sq = 0.0;
  for (int p = 0; p < kPaths; ++p) {
    const int n = poisson(poisson_rng);
    double x = drift + 0.2 * rng.Next();
    for (int j = 0; j < n; ++j) x += jump_mean + jumps.jump_vol * rng.Next();
    const double v =
        std::exp(-0.05) * std::max(100.0 * std::exp(x) - 105.0, 0.0);
    sum += v;
    sum_sq += v * v;
  }
  const double mean = sum / kPaths;
  const double se = std::sqrt((sum_sq / kPaths - mean * mean) / kPaths);
  CHECK_THAT(mean, WithinAbs(analytic, 4.0 * se));
}

TEST_CASE("Heston reduces to BSM and agrees with Monte Carlo", "[ch27]") {
  const TreeMarket m{100.0, 0.03, 0.01, 0.0};
  const HestonParams flat{0.04, 1.0, 0.04, 1e-4, 0.0};
  for (double k : {70.0, 100.0, 140.0}) {
    CHECK_THAT(
        HestonPrice(kCall, m, k, 1.0, flat),
        WithinAbs(BsmPrice(kCall, 100.0, k, 0.03, 0.01, 0.2, 1.0), 1e-6));
  }
  const HestonParams p{0.04, 2.0, 0.04, 0.5, -0.7};
  const double call = HestonPrice(kCall, m, 100.0, 1.0, p);
  CHECK_THAT(
      call - HestonPrice(kPut, m, 100.0, 1.0, p),
      WithinAbs(100.0 * std::exp(-0.01) - 100.0 * std::exp(-0.03), 1e-12));
  const auto mc = HestonMonteCarlo(kCall, m, 100.0, 1.0, p, 100, 50'000, 3);
  REQUIRE(mc.has_value());
  CHECK_THAT(mc->price, WithinAbs(call, 4.0 * mc->std_error + 0.02));
  // Negative correlation gives a downward skew: higher OTM put vols.
  const double low = HestonPrice(kPut, m, 80.0, 1.0, p);
  const double high = HestonPrice(kCall, m, 120.0, 1.0, p);
  const auto vol_low =
      ImpliedVolatility(kPut, low, 100.0, 80.0, 0.03, 0.01, 1.0);
  const auto vol_high =
      ImpliedVolatility(kCall, high, 100.0, 120.0, 0.03, 0.01, 1.0);
  REQUIRE(vol_low.has_value());
  REQUIRE(vol_high.has_value());
  CHECK(*vol_low > *vol_high + 0.03);
}

// Reference Heston call, written independently of the library: Lewis's
// integral without the control variate, std::complex arithmetic, and
// fixed 16-point Gauss-Legendre panels of width 1/4 until three in a row
// add less than 1e-16.
double ReferenceHestonCall(const TreeMarket& m, double strike, double t,
                           const HestonParams& p) {
  using Complex = std::complex<double>;
  const Complex i(0.0, 1.0);
  const double forward = m.spot * std::exp((m.rate - m.yield) * t);
  const double x = std::log(forward / strike);
  auto cf = [&](Complex xi) {  // E[exp(i xi ln(S_T / F))], little trap.
    const double s2 = p.sigma * p.sigma;
    const Complex beta = p.kappa - p.rho * p.sigma * i * xi;
    const Complex d = std::sqrt(beta * beta + s2 * (i * xi + xi * xi));
    const Complex g = (beta - d) / (beta + d);
    const Complex e = std::exp(-d * t);
    const Complex c =
        p.kappa * p.theta / s2 *
        ((beta - d) * t - 2.0 * std::log((1.0 - g * e) / (1.0 - g)));
    const Complex dd = (beta - d) / s2 * (1.0 - e) / (1.0 - g * e);
    return std::exp(c + dd * p.v0);
  };
  const auto rule = math::GaussLegendre(16, 0.0, 0.25);
  double integral = 0.0;
  int quiet = 0;
  for (int k = 0; k < 400000 && quiet < 3; ++k) {
    double part = 0.0;
    for (std::size_t j = 0; j < 16; ++j) {
      const double u = 0.25 * k + rule->nodes[j];
      part += rule->weights[j] *
              (std::exp(i * u * x) * cf(Complex(u, -0.5))).real() /
              (u * u + 0.25);
    }
    integral += part;
    quiet = std::abs(part) < 1e-16 ? quiet + 1 : 0;
  }
  return m.spot * std::exp(-m.yield * t) - std::sqrt(forward * strike) *
                                               std::exp(-m.rate * t) /
                                               std::numbers::pi * integral;
}

TEST_CASE("Heston matches an independent reference integral", "[ch27]") {
  const TreeMarket m{100.0, 0.03, 0.01, 0.0};
  struct Case {
    HestonParams p;
    double t;
  };
  const std::vector<Case> cases = {
      {{0.04, 2.0, 0.04, 0.3, -0.7}, 1.0 / 12.0},
      {{0.04, 2.0, 0.04, 0.3, -0.7}, 1.0},
      {{0.04, 2.0, 0.04, 0.3, -0.7}, 5.0},
      {{0.04, 2.0, 0.04, 1.0, -0.9}, 0.25},  // High vol of variance.
      {{0.01, 0.5, 0.09, 0.2, 0.5}, 1.0},    // Positive correlation.
      {{0.04, 0.1, 0.06, 2.0, -0.95}, 1.0},  // Feller badly violated.
      {{0.04, 0.1, 0.06, 2.0, -0.95}, 30.0},
      {{0.04, 10.0, 0.04, 1.5, -0.99}, 0.25},  // Fast mean reversion.
      {{0.09, 0.0, 0.04, 0.4, 0.9}, 10.0},     // No mean reversion.
  };
  for (const auto& c : cases) {
    for (double k : {60.0, 100.0, 140.0}) {
      CAPTURE(c.p.sigma, c.p.rho, c.t, k);
      CHECK_THAT(HestonPrice(kCall, m, k, c.t, c.p),
                 WithinAbs(ReferenceHestonCall(m, k, c.t, c.p), 1e-10));
    }
  }
  // kappa = 0 with rho = 0.9 over 30 years: the old Hull-form integral gave
  // calls below their no-arbitrage bound S e^{-qT} - K e^{-rT}.
  const HestonParams no_reversion{0.09, 0.0, 0.04, 0.4, 0.9};
  for (double k : {50.0, 100.0, 200.0}) {
    const double call = HestonPrice(kCall, m, k, 30.0, no_reversion);
    CHECK(call > 100.0 * std::exp(-0.3) - k * std::exp(-0.9));
    CHECK(call < 100.0 * std::exp(-0.3));
    CHECK_THAT(call,
               WithinAbs(ReferenceHestonCall(m, k, 30.0, no_reversion), 1e-9));
  }
}

TEST_CASE("Longstaff-Schwartz matches the binomial tree", "[ch27]") {
  // Longstaff and Schwartz (2001) Table 1: S = 36, K = 40, r = 6%, 20%.
  const TreeMarket m{36.0, 0.06, 0.0, 0.2};
  const auto lsm = LongstaffSchwartz(kPut, m, 40.0, 1.0, {});
  REQUIRE(lsm.has_value());
  const VanillaOption american{.option_type = kPut,
                               .exercise = Exercise::kAmerican,
                               .strike = 40.0,
                               .maturity = 1.0};
  const auto tree =
      PriceBinomial(american, m, 2000, TreeScheme::kCoxRossRubinstein);
  REQUIRE(tree.has_value());
  CHECK_THAT(lsm->price, WithinAbs(tree->price, 0.03));
  CHECK(lsm->price > BsmPrice(kPut, 36.0, 40.0, 0.06, 0.0, 0.2, 1.0) + 0.5);
  // An American call without dividends is European.
  const auto call = LongstaffSchwartz(kCall, m, 40.0, 1.0, {.paths = 50'000});
  REQUIRE(call.has_value());
  CHECK_THAT(call->price,
             WithinAbs(BsmPrice(kCall, 36.0, 40.0, 0.06, 0.0, 0.2, 1.0),
                       4.0 * call->std_error));
  CHECK_FALSE(LongstaffSchwartz(kPut, m, 40.0, 1.0, {.basis_degree = 0}));
}

TEST_CASE("Convertible bond tree reproduces Hull's example", "[ch27]") {
  // Example 27.1: 9-month zero-coupon, 2 shares, callable at 113.
  const TreeMarket m{50.0, 0.05, 0.0, 0.3};
  const ConvertibleTerms terms{.face = 100.0,
                               .conversion_ratio = 2.0,
                               .maturity = 0.75,
                               .call_price = 113.0,
                               .recovery = 0.4,
                               .hazard = 0.01};
  CHECK_THAT(*ConvertibleBondPrice(m, terms, 3), WithinAbs(106.93, 0.005));
  // Without the call, default or conversion value it is a riskless bond.
  ConvertibleTerms plain = terms;
  plain.call_price = std::numeric_limits<double>::infinity();
  plain.hazard = 0.0;
  plain.conversion_ratio = 1e-9;
  CHECK_THAT(*ConvertibleBondPrice(m, plain, 50),
             WithinAbs(100.0 * std::exp(-0.05 * 0.75), 1e-9));
  // Removing the issuer's call adds value.
  ConvertibleTerms non_call = terms;
  non_call.call_price = std::numeric_limits<double>::infinity();
  CHECK(*ConvertibleBondPrice(m, non_call, 100) >
        *ConvertibleBondPrice(m, terms, 100));
  ConvertibleTerms risky = terms;
  risky.hazard = 0.2;
  CHECK_FALSE(ConvertibleBondPrice(m, risky, 3));
}

}  // namespace
}  // namespace ofod::pricers
