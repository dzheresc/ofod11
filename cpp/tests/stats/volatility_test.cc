// Copyright 2026 Denis Zhereshchin
//
// EWMA, GARCH(1,1) and Ljung-Box (Hull, Ch 23).

#include "ofod/stats/volatility.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ofod/random/normal_generator.h"

namespace ofod::stats {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

std::vector<double> SimulateGarch(const Garch11& g, int n, std::uint64_t seed) {
  random::NormalGenerator rng(seed);
  double v = g.LongRunVariance();
  std::vector<double> u;
  for (int i = 0; i < n; ++i) {
    const double x = std::sqrt(v) * rng.Next();
    u.push_back(x);
    v = g.Update(v, x);
  }
  return u;
}

TEST_CASE("EWMA series starts from the seed and updates", "[stats][ch23]") {
  const std::vector<double> u = {0.01, -0.02, 0.005};
  const auto v = EwmaVariances(u, 0.94, 1e-4);
  REQUIRE(v.size() == 4);
  CHECK(v[0] == 1e-4);
  CHECK_THAT(v[3], WithinAbs(EwmaVarianceUpdate(v[2], 0.005, 0.94), 1e-18));
}

TEST_CASE("GARCH forecasts revert to the long-run variance", "[stats][ch23]") {
  const Garch11 g{.omega = 2e-6, .alpha = 0.13, .beta = 0.86};
  CHECK_THAT(g.ExpectedVariance(4e-4, 0.0), WithinRel(4e-4, 1e-14));
  CHECK_THAT(g.ExpectedVariance(4e-4, 5000.0),
             WithinRel(g.LongRunVariance(), 1e-12));
  // Long options approach sqrt(252 V_L); short ones the current vol.
  CHECK_THAT(g.TermStructureVolatility(4e-4, 1e-6),
             WithinRel(std::sqrt(252.0 * 4e-4), 1e-6));
  CHECK_THAT(g.TermStructureVolatility(4e-4, 1e6),
             WithinRel(std::sqrt(252.0 * g.LongRunVariance()), 1e-3));
}

TEST_CASE("GARCH fit recovers simulated parameters", "[stats][ch23]") {
  const Garch11 truth{.omega = 2e-6, .alpha = 0.10, .beta = 0.88};
  const auto u = SimulateGarch(truth, 5000, 11);
  const auto fit = FitGarch11(u);
  REQUIRE(fit.has_value());
  CHECK_THAT(fit->alpha, WithinAbs(truth.alpha, 0.03));
  CHECK_THAT(fit->beta, WithinAbs(truth.beta, 0.03));
  CHECK_THAT(fit->LongRunVariance(), WithinRel(truth.LongRunVariance(), 0.25));
  CHECK(fit->LogLikelihood(u) >= truth.LogLikelihood(u));
  CHECK(fit->LogLikelihood(u) > EwmaLogLikelihood(u, 0.94));
  CHECK_FALSE(FitGarch11(std::vector<double>(5, 0.01)).has_value());
  CHECK_FALSE(FitGarch11(std::vector<double>(50, 0.0)).has_value());
}

TEST_CASE("Ljung-Box is small for white noise", "[stats][ch23]") {
  random::NormalGenerator rng(5);
  const auto x = rng.Sample(2000);
  const auto q = LjungBox(x, 15);
  REQUIRE(q.has_value());
  CHECK(*q < 30.0);  // chi-square(15) 99.5% quantile is 32.8.
  CHECK_FALSE(LjungBox(x, 0).has_value());
  CHECK_FALSE(Autocorrelation(x, 2000).has_value());
  CHECK_FALSE(Autocorrelation(std::vector<double>(10, 1.0), 1).has_value());
}

TEST_CASE("Positive semi-definite check", "[stats][ch23]") {
  CHECK(IsPositiveSemidefinite({{1.0, 0.3}, {0.3, 1.0}}));
  CHECK(IsPositiveSemidefinite({{1.0, 1.0}, {1.0, 1.0}}));
  CHECK_FALSE(IsPositiveSemidefinite({{1.0, 1.2}, {1.2, 1.0}}));
  CHECK_FALSE(IsPositiveSemidefinite({{1.0, 0.0}}));
}

}  // namespace
}  // namespace ofod::stats
