// Copyright 2026 Denis Zhereshchin
//
// Value at risk, expected shortfall, backtesting and PCA (Hull, Ch 22).

#include "ofod/risk/var.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <span>
#include <vector>

#include "ofod/random/normal_generator.h"

namespace ofod::risk {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("ES exceeds VaR and both scale with sd", "[risk][ch22]") {
  for (double x : {0.95, 0.975, 0.99}) {
    CHECK(NormalExpectedShortfall(1.0, x) > NormalVar(1.0, x));
    CHECK_THAT(NormalVar(3.0, x, 1.0),
               WithinAbs(1.0 + 3.0 * NormalVar(1.0, x), 1e-12));
  }
  // The 97.5% ES of a normal is close to its 99% VaR (Basel's switch).
  CHECK_THAT(NormalExpectedShortfall(1.0, 0.975),
             WithinAbs(NormalVar(1.0, 0.99), 0.012));
}

TEST_CASE("Historical VaR needs enough scenarios", "[risk][ch22]") {
  const std::vector<double> few = {1.0, 2.0, 3.0};
  CHECK_FALSE(HistoricalVar(few, 0.99).has_value());
  CHECK_FALSE(HistoricalExpectedShortfall(few, 0.99).has_value());
  CHECK(HistoricalVar(few, 0.5) == 2.0);
}

TEST_CASE("Volatility scaling validates its inputs", "[risk][ch22]") {
  const std::vector<double> c = {0.01, 0.02};
  CHECK_FALSE(VolatilityAdjustedChanges(c, std::vector<double>{0.01}, 0.02));
  CHECK_FALSE(
      VolatilityAdjustedChanges(c, std::vector<double>{0.01, 0.0}, 0.02));
}

TEST_CASE("Portfolio sd validates sizes", "[risk][ch22]") {
  const std::vector<double> a = {1.0, 2.0};
  CHECK_FALSE(PortfolioStdDev(a, std::vector<double>{0.1}, {{1.0}}));
  CHECK_FALSE(PortfolioStdDev(a, a, {{1.0, 0.0}, {0.0}}));
  CHECK_THAT(*PortfolioStdDev(a, a, {{1.0, 1.0}, {1.0, 1.0}}),
             WithinAbs(5.0, 1e-12));
}

TEST_CASE("Delta-gamma moments reduce to normal when gamma = 0",
          "[risk][ch22]") {
  const Moments m = DeltaGammaMoments(50.0, 0.0, 0.02);
  CHECK(m.mean == 0.0);
  CHECK_THAT(m.std_dev, WithinAbs(1.0, 1e-12));
  CHECK(m.skewness == 0.0);
  CHECK_THAT(CornishFisherVar(m, 0.99), WithinAbs(NormalVar(1.0, 0.99), 1e-12));
}

TEST_CASE("Monte Carlo VaR of a linear portfolio is the normal VaR",
          "[risk][ch22]") {
  const math::Matrix cov = {{4e-4, 0.3 * 2e-2 * 1e-2},
                            {0.3 * 2e-2 * 1e-2, 1e-4}};
  const PnlFunction pnl = [](std::span<const double> dx) {
    return 10e6 * dx[0] + 5e6 * dx[1];
  };
  const auto mc = MonteCarloVar(pnl, cov, 0.99, 200000, 3);
  REQUIRE(mc.has_value());
  CHECK_THAT(mc->var, WithinRel(NormalVar(220227.0, 0.99), 0.02));
  CHECK_THAT(mc->expected_shortfall,
             WithinRel(NormalExpectedShortfall(220227.0, 0.99), 0.03));
  CHECK_FALSE(MonteCarloVar(pnl, {{1.0, 2.0}, {2.0, 1.0}}, 0.99, 1000, 1));
  CHECK_FALSE(MonteCarloVar(pnl, cov, 0.99, 10, 1));
}

TEST_CASE("Kupiec statistic and binomial tail edge cases", "[risk][ch22]") {
  CHECK_THAT(KupiecStatistic(1000, 10, 0.01), WithinAbs(0.0, 1e-12));
  CHECK(KupiecStatistic(1000, 0, 0.01) > 0.0);
  CHECK(BinomialTailProbability(100, 0, 0.01) == 1.0);
  CHECK(BinomialTailProbability(10, 11, 0.5) == 0.0);
  CHECK_THAT(BinomialTailProbability(10, 6, 0.5),
             WithinAbs(386.0 / 1024.0, 1e-12));
}

TEST_CASE("PCA explains a one-factor world with one component",
          "[risk][ch22]") {
  random::NormalGenerator rng(9);
  math::Matrix x;
  for (int i = 0; i < 400; ++i) {
    const double f = rng.Next();
    x.push_back({f, 2.0 * f, -f});
  }
  const auto pca = PrincipalComponents(x);
  REQUIRE(pca.has_value());
  CHECK(pca->variances[1] < 1e-12 * pca->variances[0]);
  const double norm = std::sqrt(6.0);
  CHECK_THAT(std::abs(pca->loadings[0][1]), WithinAbs(2.0 / norm, 1e-10));
  CHECK_FALSE(PrincipalComponents({{1.0}}).has_value());
  CHECK_FALSE(PrincipalComponents({{1.0, 2.0}, {1.0}}).has_value());
  CHECK_FALSE(PcaStdDev(std::vector<double>{1.0}, {{1.0, 2.0}},
                        std::vector<double>{1.0}));
}

}  // namespace
}  // namespace ofod::risk
