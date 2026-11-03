// Copyright 2026 Denis Zhereshchin
//
// Commodity curves and trees, weather and insurance derivatives (Hull,
// Ch 35) and real options (Ch 36).

#include "ofod/models/commodity.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/commodity_tree.h"
#include "ofod/pricers/real_options.h"
#include "ofod/pricers/weather_insurance.h"

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using instruments::Exercise;
using instruments::OptionType;
using instruments::VanillaOption;

TEST_CASE("Seasonal curve fit recovers a synthetic curve", "[ch35]") {
  models::SeasonalCurve truth{.level = std::log(3.0), .growth = 0.02};
  double sum = 0.0;
  for (std::size_t m = 0; m < 12; ++m) {
    truth.factors[m] = 0.2 * std::cos(2.0 * std::numbers::pi * m / 12.0) +
                       0.03 * static_cast<double>(m % 3);
    sum += truth.factors[m];
  }
  for (double& f : truth.factors) f -= sum / 12.0;
  std::vector<double> t;
  std::vector<double> f;
  for (int k = 0; k < 30; ++k) {
    t.push_back((k + 0.5) / 12.0);
    f.push_back(truth.Forward(t.back()));
  }
  const auto fit = models::FitSeasonalCurve(t, f);
  REQUIRE(fit.has_value());
  CHECK_THAT(fit->level, WithinAbs(truth.level, 1e-10));
  CHECK_THAT(fit->growth, WithinAbs(truth.growth, 1e-10));
  for (std::size_t m = 0; m < 12; ++m) {
    CHECK_THAT(fit->factors[m], WithinAbs(truth.factors[m], 1e-10));
  }
  CHECK_FALSE(
      models::FitSeasonalCurve(std::vector<double>(t.begin(), t.begin() + 12),
                               std::vector<double>(f.begin(), f.begin() + 12)));
}

TEST_CASE("Spike model expectation matches Monte Carlo", "[ch35]") {
  const models::SpikeModel no_jumps{
      .log_level = std::log(40.0), .a = 5.0, .sigma = 1.0};
  const double t = 0.5;
  const double x0 = 0.3;
  const double ou_var = (1.0 - std::exp(-2.0 * 5.0 * t)) / 10.0;
  CHECK_THAT(*models::SpikeExpectedPrice(no_jumps, x0, t),
             WithinRel(40.0 * std::exp(x0 * std::exp(-5.0 * t) + 0.5 * ou_var),
                       1e-12));
  const models::SpikeModel spiky{.log_level = std::log(40.0),
                                 .a = 8.0,
                                 .sigma = 0.8,
                                 .intensity = 6.0,
                                 .jump_mean = 0.8,
                                 .jump_vol = 0.3};
  const double analytic = *models::SpikeExpectedPrice(spiky, 0.0, t);
  const auto mc = models::SpikeMonteCarlo(spiky, 0.0, t, 50, 40'000, 11);
  REQUIRE(mc.has_value());
  CHECK_THAT(mc->mean, WithinAbs(analytic, 4.0 * mc->std_error));
  CHECK(analytic >
        *models::SpikeExpectedPrice(
            {.log_level = std::log(40.0), .a = 8.0, .sigma = 0.8}, 0.0, t));
  const auto path = models::SimulateSpikePath(spiky, 0.0, 1.0 / 365, 365, 2);
  REQUIRE(path.has_value());
  CHECK(path->size() == 366);
  CHECK_FALSE(models::SpikeExpectedPrice({.a = 0.0}, 0.0, 1.0));
}

TEST_CASE("Commodity tree matches futures prices and parity", "[ch35]") {
  const std::vector<double> futures = {22.0, 23.0, 24.0, 23.5, 23.0, 24.5};
  const auto tree = CommodityTree::Build(20.0, futures, 0.1, 0.2, 0.5);
  REQUIRE(tree.has_value());
  CHECK(tree->steps() == 6);
  for (int i = 1; i <= tree->steps(); ++i) {
    const auto p = tree->Probabilities(i);
    double total = 0.0;
    double expected = 0.0;
    const int w = tree->Width(i);
    for (int j = -w; j <= w; ++j) {
      const double q = p[static_cast<std::size_t>(j + w)];
      total += q;
      expected += q * tree->Price(i, j);
    }
    CHECK_THAT(total, WithinAbs(1.0, 1e-12));
    CHECK_THAT(expected,
               WithinRel(futures[static_cast<std::size_t>(i - 1)], 1e-12));
  }
  CHECK_THAT(tree->Price(0, 0), WithinRel(20.0, 1e-14));
  const double r = 0.05;
  const double call = *CommodityTreeOption(*tree, OptionType::kCall,
                                           Exercise::kEuropean, 23.0, 6, r);
  const double put = *CommodityTreeOption(*tree, OptionType::kPut,
                                          Exercise::kEuropean, 23.0, 6, r);
  CHECK_THAT(call - put, WithinAbs(std::exp(-r * 3.0) * (24.5 - 23.0), 1e-12));
  CHECK(*CommodityTreeOption(*tree, OptionType::kPut, Exercise::kAmerican, 23.0,
                             6, r) >= put);
  CHECK_FALSE(CommodityTreeOption(*tree, OptionType::kPut, Exercise::kAmerican,
                                  23.0, 7, r));
  CHECK_FALSE(CommodityTree::Build(20.0, futures, 0.0, 0.2, 0.5));
}

TEST_CASE("Degree days, payoffs and burn analysis", "[ch35]") {
  const std::vector<double> temps = {60.0, 70.0, 65.0, 50.0};
  CHECK(DegreeDays(DegreeDayKind::kHeating, temps) == 20.0);
  CHECK(DegreeDays(DegreeDayKind::kCooling, temps) == 5.0);
  const DegreeDayOption call{.type = OptionType::kCall,
                             .strike = 700.0,
                             .tick = 10'000.0,
                             .cap = 2'000'000.0};
  CHECK(DegreeDayPayoff(call, 750.0) == 500'000.0);
  CHECK(DegreeDayPayoff(call, 1'000.0) == 2'000'000.0);
  CHECK(DegreeDayPayoff(call, 650.0) == 0.0);
  const std::vector<double> seasons = {650.0, 720.0, 800.0, 690.0};
  const auto burn = BurnAnalysis(call, seasons, 0.05, 0.5);
  REQUIRE(burn.has_value());
  CHECK_THAT(burn->value, WithinRel(std::exp(-0.025) * 300'000.0, 1e-12));
  CHECK_FALSE(BurnAnalysis(call, std::vector<double>{700.0}, 0.05, 0.5));
}

TEST_CASE("Temperature model fit and degree-day simulation", "[ch35]") {
  const TemperatureModel truth{.mean = 55.0,
                               .trend = 0.001,
                               .amplitude = 20.0,
                               .phase = 200.0,
                               .phi = 0.7,
                               .sigma = 4.0};
  std::mt19937_64 engine(3);
  std::normal_distribution<double> normal;
  std::vector<double> temps;
  double x = 0.0;
  for (int d = 0; d < 365 * 20; ++d) {
    x = truth.phi * x + truth.sigma * normal(engine);
    temps.push_back(truth.Seasonal(d) + x);
  }
  const auto fit = FitTemperatureModel(temps);
  REQUIRE(fit.has_value());
  CHECK_THAT(fit->mean, WithinAbs(truth.mean, 1.0));
  CHECK_THAT(fit->amplitude, WithinAbs(truth.amplitude, 0.3));
  CHECK_THAT(fit->phase, WithinAbs(truth.phase, 2.0));
  CHECK_THAT(fit->phi, WithinAbs(truth.phi, 0.03));
  CHECK_THAT(fit->sigma, WithinAbs(truth.sigma, 0.1));
  // With no noise the season's degree days are deterministic.
  TemperatureModel quiet = truth;
  quiet.sigma = 0.0;
  const SeasonSpec winter{
      .kind = DegreeDayKind::kHeating, .first_day = 7300 + 334, .days = 90};
  std::vector<double> path;
  for (int d = 0; d < winter.days; ++d) {
    path.push_back(quiet.Seasonal(winter.first_day + d));
  }
  const double dd = DegreeDays(DegreeDayKind::kHeating, path);
  const DegreeDayOption call{.strike = dd - 100.0, .tick = 1.0};
  const auto sim =
      SimulateDegreeDayOption(quiet, winter, call, 0.0, 0.25, 10, 1);
  REQUIRE(sim.has_value());
  CHECK_THAT(sim->mean_degree_days, WithinAbs(dd, 1e-9));
  CHECK_THAT(sim->option.value, WithinAbs(100.0, 1e-9));
  const auto noisy =
      SimulateDegreeDayOption(truth, winter, call, 0.0, 0.25, 20'000, 2);
  REQUIRE(noisy.has_value());
  CHECK(noisy->std_degree_days > 50.0);
}

TEST_CASE("Catastrophe bonds and reinsurance layers", "[ch35]") {
  const CatBondTerms bond{.principal = 100.0,
                          .coupon = 0.08,
                          .maturity = 3.0,
                          .attachment = 500.0,
                          .exhaustion = 1'000.0,
                          .frequency = 2};
  const double r = 0.03;
  double riskless = 100.0 * std::exp(-r * 3.0);
  for (int k = 1; k <= 6; ++k) riskless += 4.0 * std::exp(-r * k / 2.0);
  const auto safe = PriceCatBond(bond, {.intensity = 0.0}, r, 100, 1);
  REQUIRE(safe.has_value());
  CHECK_THAT(safe->price, WithinRel(riskless, 1e-12));
  CHECK_THAT(safe->spread, WithinAbs(0.0, 1e-9));
  const LossModel hurricanes{
      .intensity = 0.3, .severity_mu = std::log(300.0), .severity_sigma = 0.8};
  const auto cat = PriceCatBond(bond, hurricanes, r, 100'000, 7);
  REQUIRE(cat.has_value());
  CHECK(cat->price < riskless);
  CHECK(cat->spread > 0.0);
  CHECK_THAT(riskless - cat->price,
             WithinAbs(100.0 * std::exp(-r * 3.0) * cat->expected_loss, 1e-9));
  // With attachment 0 any event triggers: P = 1 - exp(-lambda T).
  CatBondTerms first_loss = bond;
  first_loss.attachment = 0.0;
  const auto first = PriceCatBond(first_loss, hurricanes, r, 100'000, 8);
  REQUIRE(first.has_value());
  CHECK_THAT(first->loss_probability, WithinAbs(1.0 - std::exp(-0.9), 0.006));
  // Fixed severities: the layer's expected payout is a Poisson sum.
  const LossModel fixed{.intensity = 2.0, .severity_mu = std::log(10.0)};
  double exact = 0.0;
  double prob = std::exp(-2.0);
  for (int n = 0; n < 40; ++n) {
    exact += prob * std::min(std::max(10.0 * n - 15.0, 0.0), 20.0);
    prob *= 2.0 / (n + 1);
  }
  const auto layer = ExcessOfLossLayer(fixed, 15.0, 20.0, 1.0, 0.0, 200'000, 5);
  REQUIRE(layer.has_value());
  CHECK_THAT(layer->value, WithinAbs(exact, 4.0 * layer->std_error));
  CHECK_FALSE(PriceCatBond({.attachment = 10.0, .exhaustion = 5.0}, hurricanes,
                           r, 100, 1));
}

TEST_CASE("Market price of risk and risk-neutral present values", "[ch36]") {
  // Hull's Ch 36.2 style: rho 0.7, market excess 6%, market vol 20%.
  CHECK_THAT(MarketPriceOfRisk(0.7, 0.10, 0.04, 0.20), WithinAbs(0.21, 1e-12));
  CHECK_THAT(RiskNeutralGrowth(0.08, 0.21, 0.3), WithinAbs(0.017, 1e-12));
  const std::vector<double> times = {1.0, 2.0};
  const std::vector<double> flows = {100.0, 110.0};
  CHECK_THAT(*RiskNeutralPresentValue(times, flows, 0.2, 0.25, 0.05),
             WithinRel(100.0 * std::exp(-0.1) + 110.0 * std::exp(-0.2), 1e-12));
}

TEST_CASE("Project options on a lattice", "[ch36]") {
  const ProjectLattice lattice{.value = 100.0,
                               .volatility = 0.3,
                               .rate = 0.05,
                               .payout = 0.04,
                               .maturity = 2.0,
                               .steps = 200};
  CHECK_THAT(*ProjectWithOptions(lattice, {}), WithinRel(100.0, 1e-12));
  // Abandonment is an American put on V: project + put.
  const std::array<ProjectOption, 1> abandon = {Abandon{90.0}};
  const VanillaOption put{.option_type = OptionType::kPut,
                          .exercise = Exercise::kAmerican,
                          .strike = 90.0,
                          .maturity = 2.0};
  const TreeMarket market{100.0, 0.05, 0.04, 0.3};
  const auto tree_put =
      PriceBinomial(put, market, 200, TreeScheme::kCoxRossRubinstein);
  REQUIRE(tree_put.has_value());
  CHECK_THAT(*ProjectWithOptions(lattice, abandon),
             WithinRel(100.0 + tree_put->price, 1e-10));
  // Expansion by 30% for 25: a call on 0.3 V with strike 25.
  const std::array<ProjectOption, 1> expand = {Expand{1.3, 25.0}};
  const double with_expand = *ProjectWithOptions(lattice, expand);
  CHECK(with_expand > 100.0);
  // Several options together are worth at least each one alone.
  const std::array<ProjectOption, 3> all = {Abandon{90.0}, Expand{1.3, 25.0},
                                            Contract{0.7, 25.0}};
  const double combined = *ProjectWithOptions(lattice, all);
  CHECK(combined >= with_expand);
  CHECK(combined >= 100.0 + tree_put->price);
  const VanillaOption call{.option_type = OptionType::kCall,
                           .exercise = Exercise::kAmerican,
                           .strike = 110.0,
                           .maturity = 2.0};
  CHECK_THAT(
      *DeferralOption(lattice, 110.0),
      WithinRel(PriceBinomial(call, market, 200, TreeScheme::kCoxRossRubinstein)
                    ->price,
                1e-10));
  CHECK_FALSE(ProjectWithOptions({.value = 100.0}, abandon));
}

TEST_CASE("Extraction project on the commodity tree", "[ch36]") {
  const std::vector<double> futures = {22.0, 23.0, 24.0};
  const auto tree = CommodityTree::Build(20.0, futures, 0.1, 0.2, 1.0);
  REQUIRE(tree.has_value());
  const ExtractionProject project{.production = 2.0,
                                  .variable_cost = 17.0,
                                  .fixed_cost = 6.0,
                                  .rate = 0.1,
                                  .abandon_value = 0.0,
                                  .expand_factor = 1.2,
                                  .expand_cost = 2.0};
  const auto v = ValueExtractionProject(*tree, project);
  REQUIRE(v.has_value());
  double pv = 0.0;
  for (int i = 1; i <= 3; ++i) {
    pv += std::exp(-0.1 * i) *
          (2.0 * (futures[static_cast<std::size_t>(i - 1)] - 17.0) - 6.0);
  }
  CHECK_THAT(v->base, WithinAbs(pv, 1e-12));
  CHECK(v->with_abandon > v->base);
  CHECK(v->with_expand >= v->base);
}

TEST_CASE("Business valuation by Monte Carlo", "[ch36]") {
  BusinessSpec spec{.revenue = 50.0,
                    .growth = 0.03,
                    .volatility = 0.0,
                    .variable_cost = 0.6,
                    .fixed_cost = 15.0,
                    .cash = 10.0,
                    .rate = 0.04,
                    .years = 5.0,
                    .steps_per_year = 4,
                    .terminal_multiple = 8.0};
  // No volatility: a deterministic recursion.
  double revenue = 50.0;
  double cash = 10.0;
  double flow = 0.0;
  const double dt = 0.25;
  for (int i = 0; i < 20; ++i) {
    revenue *= std::exp(0.03 * dt);
    flow = revenue * 0.4 - 15.0;
    cash = cash * std::exp(0.04 * dt) + flow * dt;
  }
  const auto flat = ValueBusiness(spec, 10, 1);
  REQUIRE(flat.has_value());
  CHECK_THAT(flat->value,
             WithinRel(std::exp(-0.2) * (cash + 8.0 * flow), 1e-10));
  CHECK(flat->bankruptcy_probability == 0.0);
  spec.volatility = 0.4;
  const auto risky = ValueBusiness(spec, 20'000, 2);
  REQUIRE(risky.has_value());
  CHECK(risky->bankruptcy_probability > 0.05);
  CHECK_FALSE(ValueBusiness({.revenue = 0.0}, 10, 1));
}

}  // namespace
}  // namespace ofod::pricers
