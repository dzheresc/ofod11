// Copyright 2026 Denis Zhereshchin
//
// Credit curves, Merton, copulas, CDS, CDO tranches and XVA (Hull, Ch 9,
// 24, 25).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include "ofod/credit/cds.h"
#include "ofod/credit/copula.h"
#include "ofod/credit/credit_risk.h"
#include "ofod/credit/xva.h"
#include "ofod/curves/credit_curve.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/math/normal.h"
#include "ofod/math/quadrature.h"
#include "ofod/pricers/black_scholes.h"

namespace ofod::credit {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using curves::CreditCurve;
using curves::ZeroCurve;

TEST_CASE("Bivariate normal matches numerical integration", "[math][ch24]") {
  for (double rho : {-0.95, -0.5, 0.0, 0.3, 0.8, 0.97}) {
    for (auto [a, b] :
         {std::pair{-1.0, 0.5}, std::pair{0.7, 1.3}, std::pair{-2.0, -1.5}}) {
      // M(a, b) = int_{-inf}^{a} phi(x) N((b - rho x) / sqrt(1 - rho^2)).
      double sum = 0.0;
      constexpr double kH = 1e-3;
      for (double x = -9.0; x < a; x += kH) {
        const double m = x + 0.5 * kH;
        sum += math::NormPdf(m) *
               math::NormCdf((b - rho * m) / std::sqrt(1.0 - rho * rho)) * kH;
      }
      CHECK_THAT(math::BivariateNormCdf(a, b, rho), WithinAbs(sum, 2e-6));
    }
  }
  CHECK_THAT(math::BivariateNormCdf(0.4, 0.9, 1.0),
             WithinAbs(math::NormCdf(0.4), 1e-14));
  CHECK_THAT(math::BivariateNormCdf(0.4, 0.9, -1.0),
             WithinAbs(math::NormCdf(0.4) + math::NormCdf(0.9) - 1.0, 1e-14));
}

TEST_CASE("Gauss-Hermite integrates polynomials exactly", "[math][ch25]") {
  const auto q = math::GaussHermiteNormal(20);
  REQUIRE(q.has_value());
  double m0 = 0.0;
  double m2 = 0.0;
  double m4 = 0.0;
  for (std::size_t i = 0; i < q->nodes.size(); ++i) {
    const double x = q->nodes[i];
    m0 += q->weights[i];
    m2 += q->weights[i] * x * x;
    m4 += q->weights[i] * x * x * x * x;
  }
  CHECK_THAT(m0, WithinAbs(1.0, 1e-13));
  CHECK_THAT(m2, WithinAbs(1.0, 1e-13));
  CHECK_THAT(m4, WithinAbs(3.0, 1e-12));
  CHECK_FALSE(math::GaussHermite(0).has_value());
  CHECK(math::GaussHermite(1)->nodes[0] == 0.0);
}

TEST_CASE("Credit curve integrates piecewise hazards", "[curves][ch24]") {
  const auto c = CreditCurve::Create({1.0, 3.0}, {0.01, 0.03});
  REQUIRE(c.has_value());
  CHECK(c->SurvivalProbability(0.0) == 1.0);
  CHECK_THAT(c->SurvivalProbability(2.0), WithinAbs(std::exp(-0.04), 1e-15));
  CHECK_THAT(c->SurvivalProbability(5.0), WithinAbs(std::exp(-0.13), 1e-15));
  CHECK(c->Hazard(0.5) == 0.01);
  CHECK(c->Hazard(9.0) == 0.03);
  CHECK_FALSE(CreditCurve::Create({}, {}).has_value());
  CHECK_FALSE(CreditCurve::Create({1.0, 1.0}, {0.01, 0.01}).has_value());
  CHECK_FALSE(CreditCurve::Create({1.0}, {-0.01}).has_value());
}

TEST_CASE("Merton solution reprices equity", "[credit][ch24]") {
  const auto m = MertonSolve(3.0, 0.8, 10.0, 0.05, 1.0);
  REQUIRE(m.has_value());
  CHECK_THAT(pricers::BsmPrice(instruments::OptionType::kCall, m->asset_value,
                               10.0, 0.05, 0.0, m->asset_volatility, 1.0),
             WithinAbs(3.0, 1e-9));
  CHECK_FALSE(MertonSolve(0.0, 0.8, 10.0, 0.05, 1.0).has_value());
}

TEST_CASE("WCDR equals PD without correlation", "[credit][ch24]") {
  CHECK_THAT(Wcdr(0.02, 0.0, 0.999), WithinAbs(0.02, 1e-15));
  CHECK_THAT(JointDefaultProbability(0.02, 0.03, 0.0),
             WithinAbs(0.02 * 0.03, 1e-14));
}

const ZeroCurve kDiscount = ZeroCurve::Flat(0.04);

TEST_CASE("CDS at the par spread is worth zero", "[cds][ch25]") {
  const CdsTerms terms{.maturity = 5.0, .frequency = 4, .recovery = 0.4};
  const CreditCurve curve = CreditCurve::Flat(0.015);
  const auto s = CdsParSpread(terms, curve, kDiscount);
  REQUIRE(s.has_value());
  // The credit triangle: spread ~ lambda (1 - R).
  CHECK_THAT(*s, WithinRel(0.015 * 0.6, 0.01));
  CHECK_THAT(*CdsBuyerValue(terms, *s, curve, kDiscount), WithinAbs(0, 1e-15));
  CHECK_THAT(*CdsUpfront(terms, *s, curve, kDiscount), WithinAbs(0, 1e-15));
  CHECK_FALSE(CdsLegValues({.maturity = 0.0}, curve, kDiscount).has_value());
}

TEST_CASE("Bootstrapping flat spreads gives a flat hazard", "[cds][ch25]") {
  const CdsTerms terms{.maturity = 5.0, .frequency = 4, .recovery = 0.4};
  const double s = *CdsParSpread(terms, CreditCurve::Flat(0.02), kDiscount);
  const std::vector<double> mats = {1.0, 2.0, 5.0};
  const std::vector<double> spreads(3, s);
  const auto c = BootstrapCreditCurve(mats, spreads, 4, 0.4, kDiscount);
  REQUIRE(c.has_value());
  for (double h : c->hazards()) CHECK_THAT(h, WithinAbs(0.02, 2e-4));
  CHECK_FALSE(
      BootstrapCreditCurve(mats, std::vector<double>{s}, 4, 0.4, kDiscount));
}

TEST_CASE("CDS options satisfy put-call parity", "[cds][ch25]") {
  const double payer = CdsOptionPrice(true, 0.012, 0.01, 0.5, 1.0, 3.5);
  const double receiver = CdsOptionPrice(false, 0.012, 0.01, 0.5, 1.0, 3.5);
  CHECK_THAT(payer - receiver, WithinAbs(3.5 * (0.012 - 0.01), 1e-14));
}

TEST_CASE("Default count recursion is a distribution", "[copula][ch25]") {
  const std::vector<double> p = {0.1, 0.3, 0.5, 0.05};
  const auto d = DefaultCountDistribution(p);
  REQUIRE(d.size() == 5);
  double total = 0.0;
  double mean = 0.0;
  for (std::size_t k = 0; k < d.size(); ++k) {
    total += d[k];
    mean += k * d[k];
  }
  CHECK_THAT(total, WithinAbs(1.0, 1e-15));
  CHECK_THAT(mean, WithinAbs(0.95, 1e-15));
  CHECK_THAT(d[4], WithinAbs(0.1 * 0.3 * 0.5 * 0.05, 1e-17));
}

TEST_CASE("Tranche losses add up to the pool's", "[copula][ch25]") {
  const CdoPool pool{.names = 50, .hazard = 0.02, .recovery = 0.4};
  const std::vector<Tranche> slices = {
      {0.0, 0.03}, {0.03, 0.07}, {0.07, 0.15}, {0.15, 1.0}};
  double weighted = 0.0;
  for (const auto& t : slices) {
    weighted +=
        (t.detachment - t.attachment) * *ExpectedTrancheLoss(t, pool, 0.3, 5.0);
  }
  const double pool_loss = 0.6 * (1.0 - std::exp(-0.1));
  CHECK_THAT(weighted, WithinAbs(pool_loss, 1e-12));
  // Correlation moves risk from the equity tranche to senior tranches.
  CHECK(*ExpectedTrancheLoss({0.0, 0.03}, pool, 0.6, 5.0) <
        *ExpectedTrancheLoss({0.0, 0.03}, pool, 0.1, 5.0));
  CHECK(*ExpectedTrancheLoss({0.15, 1.0}, pool, 0.6, 5.0) >
        *ExpectedTrancheLoss({0.15, 1.0}, pool, 0.1, 5.0));
  CHECK_FALSE(ExpectedTrancheLoss({0.1, 0.05}, pool, 0.3, 5.0).has_value());
  CHECK_FALSE(ExpectedTrancheLoss({0.0, 0.03}, pool, 1.0, 5.0).has_value());
}

TEST_CASE("Implied correlation inverts the tranche spread", "[copula][ch25]") {
  const CdoPool pool{.names = 50, .hazard = 0.015, .recovery = 0.4};
  const CdoTerms terms{
      .maturity = 5.0, .frequency = 4, .quadrature_points = 40};
  const Tranche equity{0.0, 0.03};
  const double s = *TrancheSpread(equity, pool, 0.25, terms, kDiscount);
  const auto rho = ImpliedTrancheCorrelation(equity, pool, s, terms, kDiscount);
  REQUIRE(rho.has_value());
  CHECK_THAT(*rho, WithinAbs(0.25, 1e-8));
  CHECK_FALSE(ImpliedTrancheCorrelation(equity, pool, 10.0, terms, kDiscount));
}

TEST_CASE("A one-name basket is a CDS", "[copula][ch25]") {
  const auto b = NthToDefaultSpread({.names = 1,
                                     .nth = 1,
                                     .hazard = 0.02,
                                     .recovery = 0.4,
                                     .correlation = 0.3,
                                     .maturity = 5.0,
                                     .frequency = 4},
                                    0.04, 100000, 5);
  REQUIRE(b.has_value());
  const double cds =
      *CdsParSpread({.maturity = 5.0, .frequency = 4, .recovery = 0.4},
                    CreditCurve::Flat(0.02), kDiscount);
  CHECK_THAT(b->spread, WithinAbs(cds, 4.0 * b->std_error + 2e-5));
  CHECK_FALSE(NthToDefaultSpread({.names = 3, .nth = 4}, 0.04, 100, 1));
}

TEST_CASE("Forward exposure matches its option value", "[xva][ch09]") {
  const auto p =
      SimulateForwardExposure(100.0, 95.0, 0.03, 0.25, 2.0, 8, 100000, 9);
  REQUIRE(p.has_value());
  for (std::size_t j = 0; j < p->times.size(); ++j) {
    const double t = p->times[j];
    // Discounted E[max(S_t - K e^{-r(T-t)}, 0)] is a BSM call.
    const double analytic = pricers::BsmPrice(
        instruments::OptionType::kCall, 100.0,
        95.0 * std::exp(-0.03 * (2.0 - t)), 0.03, 0.0, 0.25, t);
    CHECK_THAT(p->expected_exposure[j], WithinRel(analytic, 0.02));
  }
  const auto capped = SimulateForwardExposure(100.0, 95.0, 0.03, 0.25, 2.0, 8,
                                              20000, 9, 0.975, 5.0);
  REQUIRE(capped.has_value());
  for (double pfe : capped->potential_future_exposure) CHECK(pfe <= 5.0);
  CHECK_FALSE(ProfileFromPaths(std::vector<double>{1.0}, {{1.0}}));
  CHECK_FALSE(ProfileFromPaths(std::vector<double>{1.0}, {{1.0}, {1.0, 2.0}}));
}

TEST_CASE("CVA and DVA validate sizes and scale with LGD", "[xva][ch09]") {
  const std::vector<double> t = {1.0, 2.0};
  const std::vector<double> e = {5.0, 4.0};
  const CreditCurve c = CreditCurve::Flat(0.03);
  CHECK_THAT(*Cva(t, e, c, 0.2) / *Cva(t, e, c, 0.6), WithinAbs(2.0, 1e-14));
  CHECK(*Dva(t, e, c, 0.4) == *Cva(t, e, c, 0.4));
  CHECK_FALSE(Cva(t, std::vector<double>{1.0}, c, 0.4).has_value());
  CHECK_FALSE(
      FundingValueAdjustment(t, std::vector<double>{1.0}, 0.01).has_value());
}

}  // namespace
}  // namespace ofod::credit
