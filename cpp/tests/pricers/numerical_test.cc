// Copyright 2026 Denis Zhereshchin
//
// Tree, Monte Carlo and finite difference engines (Hull, Ch 21).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <span>
#include <vector>

#include "ofod/math/normal.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/finite_difference.h"
#include "ofod/pricers/monte_carlo.h"
#include "ofod/random/sobol.h"

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using instruments::Exercise;
using instruments::OptionType;
using instruments::VanillaOption;

const VanillaOption kCall{.strike = 40.0, .maturity = 0.5};
const TreeMarket kMarket{
    .spot = 42.0, .rate = 0.10, .yield = 0.02, .volatility = 0.20};
double Bsm() {
  return BsmPrice(OptionType::kCall, 42, 40, 0.10, 0.02, 0.2, 0.5);
}

TEST_CASE("NormInv inverts NormCdf", "[math][ch21]") {
  for (double p : {1e-12, 1e-6, 0.01, 0.3, 0.5, 0.77, 0.99, 1 - 1e-9}) {
    CHECK_THAT(math::NormCdf(math::NormInv(p)), WithinAbs(p, 1e-15));
  }
  CHECK(std::isinf(math::NormInv(0.0)));
  CHECK(std::isnan(math::NormInv(1.5)));
}

TEST_CASE("Sobol points fill the unit cube evenly", "[random][ch21]") {
  CHECK_FALSE(random::Sobol::Create(0));
  CHECK_FALSE(random::Sobol::Create(9));
  auto sobol = random::Sobol::Create(8);
  REQUIRE(sobol.has_value());
  const int n = 4095;  // 2^12 - 1 points: every dyadic box is hit evenly.
  std::vector<double> mean(8, 0.0);
  for (int i = 0; i < n; ++i) {
    const auto x = sobol->Next();
    for (std::size_t d = 0; d < 8; ++d) {
      REQUIRE(x[d] >= 0.0);
      REQUIRE(x[d] < 1.0);
      mean[d] += x[d] / n;
    }
  }
  for (double m : mean) CHECK_THAT(m, WithinAbs(0.5, 1e-3));
}

TEST_CASE("Trinomial and CRR trees agree with BSM", "[lattice][ch21]") {
  const auto tri = PriceTrinomial(kCall, kMarket, 400);
  REQUIRE(tri.has_value());
  CHECK_THAT(tri->price, WithinAbs(Bsm(), 2e-3));
  CHECK_FALSE(PriceTrinomial(kCall, kMarket, 0));
}

TEST_CASE("Dividend tree with no dividends is the plain tree",
          "[lattice][ch21]") {
  const TreeMarket m{.spot = 50, .rate = 0.05, .volatility = 0.3};
  const VanillaOption put{.option_type = OptionType::kPut,
                          .exercise = Exercise::kAmerican,
                          .strike = 52,
                          .maturity = 1.0};
  const std::vector<instruments::CashFlow> none;
  const auto a = PriceBinomialWithDividends(put, m, 100, none);
  const auto b = PriceBinomial<CoxRossRubinstein>(put, m, 100);
  REQUIRE(a.has_value());
  REQUIRE(b.has_value());
  CHECK_THAT(a->price, WithinAbs(b->price, 1e-12));
  CHECK_FALSE(PriceBinomialWithDividends(put, kMarket, 100, none));  // q != 0.
}

TEST_CASE("Every Monte Carlo method agrees with BSM", "[mc][ch21]") {
  const double target = Bsm();
  double plain_se = 0.0;
  for (auto method :
       {VarianceReduction::kNone, VarianceReduction::kAntithetic,
        VarianceReduction::kControlVariate,
        VarianceReduction::kImportanceSampling, VarianceReduction::kStratified,
        VarianceReduction::kMomentMatching, VarianceReduction::kQuasiRandom}) {
    const auto r = MonteCarloEuropean(
        kCall, kMarket, {.paths = 64000, .seed = 11, .method = method});
    REQUIRE(r.has_value());
    CHECK_THAT(r->price, WithinAbs(target, 4.0 * r->std_error + 1e-4));
    if (method == VarianceReduction::kNone) {
      plain_se = r->std_error;
    } else if (method != VarianceReduction::kImportanceSampling) {
      CHECK(r->std_error < plain_se);
    }
  }
  VanillaOption american = kCall;
  american.exercise = Exercise::kAmerican;
  CHECK_FALSE(MonteCarloEuropean(american, kMarket, {}));
}

TEST_CASE("Importance sampling helps deep out-of-the-money calls",
          "[mc][ch21]") {
  const VanillaOption otm{.strike = 55.0, .maturity = 0.5};
  const auto plain = MonteCarloEuropean(otm, kMarket, {.paths = 50000});
  const auto is = MonteCarloEuropean(
      otm, kMarket,
      {.paths = 50000, .method = VarianceReduction::kImportanceSampling});
  REQUIRE(plain.has_value());
  REQUIRE(is.has_value());
  CHECK(is->std_error < 0.25 * plain->std_error);
  CHECK_THAT(is->price, WithinAbs(BsmPrice(OptionType::kCall, 42, 55, 0.10,
                                           0.02, 0.2, 0.5),
                                  4.0 * is->std_error));
}

TEST_CASE("Path simulator is independent of the thread count", "[mc][ch21]") {
  auto payoff = [](std::span<const double> path) {
    double sum = 0.0;
    for (double s : path.subspan(1)) sum += s;
    return std::max(sum / static_cast<double>(path.size() - 1) - 40.0, 0.0);
  };
  const McOptions one{.paths = 20000, .seed = 5, .threads = 1};
  McOptions four = one;
  four.threads = 4;
  const auto a = SimulateGbmPaths(kMarket, 0.5, 12, payoff, one);
  const auto b = SimulateGbmPaths(kMarket, 0.5, 12, payoff, four);
  REQUIRE(a.has_value());
  REQUIRE(b.has_value());
  CHECK(a->price == b->price);
  CHECK(a->std_error == b->std_error);
  // A one-step path simulation of a vanilla payoff matches BSM.
  auto vanilla = [](std::span<const double> path) {
    return std::max(path.back() - 40.0, 0.0);
  };
  const auto v = SimulateGbmPaths(kMarket, 0.5, 1, vanilla,
                                  {.paths = 100000,
                                   .method = VarianceReduction::kAntithetic,
                                   .threads = 3});
  REQUIRE(v.has_value());
  CHECK_THAT(v->price, WithinAbs(Bsm(), 4.0 * v->std_error));
  CHECK_FALSE(SimulateGbmPaths(kMarket, 0.5, 1, vanilla,
                               {.method = VarianceReduction::kStratified}));
}

TEST_CASE("Finite differences converge to BSM", "[fd][ch21]") {
  VanillaOption put{
      .option_type = OptionType::kPut, .strike = 40.0, .maturity = 0.5};
  const double bsm = BsmPrice(OptionType::kPut, 42, 40, 0.10, 0.02, 0.2, 0.5);
  const FdGrid s_grid{.s_max = 160, .price_steps = 400, .time_steps = 200};
  const FdGrid log_grid{.s_min = 2,
                        .s_max = 400,
                        .price_steps = 400,
                        .time_steps = 200,
                        .log_space = true};
  for (const auto& grid : {s_grid, log_grid}) {
    for (auto scheme : {FdScheme::kImplicit, FdScheme::kCrankNicolson}) {
      const auto v = PriceFiniteDifference(put, kMarket, grid, scheme);
      REQUIRE(v.has_value());
      CHECK_THAT(*v, WithinAbs(bsm, 5e-3));
    }
  }
  // Explicit needs dt < dS^2 / (sigma S_max)^2: dS = 0.8, dt = 2.5e-4.
  const FdGrid stable{.s_max = 160, .price_steps = 200, .time_steps = 2000};
  const auto e =
      PriceFiniteDifference(put, kMarket, stable, FdScheme::kExplicit);
  REQUIRE(e.has_value());
  CHECK_THAT(*e, WithinAbs(bsm, 5e-3));
  // American put: PSOR and the tree agree.
  put.exercise = Exercise::kAmerican;
  const auto fd =
      PriceFiniteDifference(put, kMarket, s_grid, FdScheme::kCrankNicolson);
  const auto tree = PriceBinomial<CoxRossRubinstein>(put, kMarket, 2000);
  REQUIRE(fd.has_value());
  REQUIRE(tree.has_value());
  CHECK_THAT(*fd, WithinAbs(tree->price, 5e-3));
  CHECK_FALSE(
      PriceFiniteDifference(put, kMarket, {.s_max = 30}, FdScheme::kImplicit));
  CHECK_FALSE(PriceFiniteDifference(
      put, kMarket, {.s_min = 0.0, .s_max = 100, .log_space = true},
      FdScheme::kImplicit));
}

}  // namespace
}  // namespace ofod::pricers
