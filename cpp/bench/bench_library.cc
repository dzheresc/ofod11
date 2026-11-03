// Copyright 2026 Denis Zhereshchin
//
// Google Benchmark suite for the rest of the library (the M12 pass):
// American finite differences, closed-form exotics, credit (CDS, CDO
// tranches, baskets, exposure), rate curves and options, Black-Karasinski
// trees, mortgage-backed securities, commodities, real options, market
// risk and Greeks. Built into the same ofod_bench executable as
// bench_pricers.cc.

#include <benchmark/benchmark.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

#include "ofod/credit/cds.h"
#include "ofod/credit/copula.h"
#include "ofod/credit/tranche.h"
#include "ofod/credit/xva.h"
#include "ofod/curves/credit_curve.h"
#include "ofod/curves/swap_bootstrap.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/greeks/greeks.h"
#include "ofod/math/linalg.h"
#include "ofod/models/commodity.h"
#include "ofod/models/short_rate.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/commodity_tree.h"
#include "ofod/pricers/exotics.h"
#include "ofod/pricers/finite_difference.h"
#include "ofod/pricers/mbs.h"
#include "ofod/pricers/rate_options.h"
#include "ofod/pricers/real_options.h"
#include "ofod/pricers/short_rate_tree.h"
#include "ofod/risk/var.h"
#include "ofod/stats/volatility.h"

namespace ofod {
namespace {

using instruments::Exercise;
using instruments::OptionType;
using instruments::VanillaOption;
using pricers::TreeMarket;

const VanillaOption kAmericanPut{.option_type = OptionType::kPut,
                                 .exercise = Exercise::kAmerican,
                                 .strike = 50.0,
                                 .maturity = 5.0 / 12.0};
const TreeMarket kMarket{.spot = 50.0, .rate = 0.10, .volatility = 0.40};

// ---------------------------------------------------------------------------
// Finite differences: PSOR, the projection solver and Brennan-Schwartz
// (Ch 21.8).

void BmFdAmericanSolver(benchmark::State& state) {
  const auto scheme = static_cast<pricers::FdScheme>(state.range(0));
  const auto solver = static_cast<pricers::AmericanSolver>(state.range(1));
  const pricers::FdGrid grid{
      .s_max = 200, .price_steps = 400, .time_steps = 400};
  for (auto _ : state) {
    benchmark::DoNotOptimize(pricers::PriceFiniteDifference(
        kAmericanPut, kMarket, grid, scheme, solver));
  }
}
BENCHMARK(BmFdAmericanSolver)
    ->ArgsProduct(
        {{static_cast<int>(pricers::FdScheme::kImplicit),
          static_cast<int>(pricers::FdScheme::kCrankNicolson)},
         {static_cast<int>(pricers::AmericanSolver::kPsor),
          static_cast<int>(pricers::AmericanSolver::kProjection),
          static_cast<int>(pricers::AmericanSolver::kBrennanSchwartz)}})
    ->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// Exotic closed forms (Ch 26) and the variance swap strip.

void BmExoticFormulas(benchmark::State& state) {
  const TreeMarket m{
      .spot = 100.0, .rate = 0.08, .yield = 0.04, .volatility = 0.25};
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        pricers::ArithmeticAsianPrice(OptionType::kCall, m, 100.0, 1.0));
    benchmark::DoNotOptimize(
        pricers::GeometricAsianPrice(OptionType::kCall, m, 100.0, 1.0));
    benchmark::DoNotOptimize(
        pricers::FloatingLookbackPrice(OptionType::kCall, m, 100.0, 1.0));
    benchmark::DoNotOptimize(
        pricers::FixedLookbackPrice(OptionType::kCall, m, 100.0, 100.0, 1.0));
    benchmark::DoNotOptimize(pricers::ChooserPrice(m, 100.0, 0.5, 1.0));
    benchmark::DoNotOptimize(
        pricers::GapOptionPrice(OptionType::kCall, m, 100.0, 105.0, 1.0));
  }
}
BENCHMARK(BmExoticFormulas)->Unit(benchmark::kMicrosecond);

void BmVarianceSwap(benchmark::State& state) {
  const TreeMarket m{.spot = 100.0, .rate = 0.04, .yield = 0.0};
  const int n = static_cast<int>(state.range(0));
  std::vector<double> strikes;
  for (int i = 0; i < n; ++i) strikes.push_back(50.0 + 100.0 * i / (n - 1));
  const pricers::VolatilityFunction smile = [](double k) {
    return 0.2 + 0.1 * std::abs(std::log(k / 100.0));
  };
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        pricers::VarianceSwapFairVariance(m, 1.0, strikes, smile));
  }
}
BENCHMARK(BmVarianceSwap)->Arg(101)->Arg(1001)->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// Credit (Ch 9, 24, 25).

const curves::ZeroCurve kDiscount = curves::ZeroCurve::Flat(0.05);

void BmCdsParSpread(benchmark::State& state) {
  const auto credit = *curves::CreditCurve::Create(
      {1, 3, 5, 7, 10}, {0.01, 0.012, 0.015, 0.017, 0.02});
  const credit::CdsTerms terms{.maturity = 10.0};
  for (auto _ : state) {
    benchmark::DoNotOptimize(credit::CdsParSpread(terms, credit, kDiscount));
  }
}
BENCHMARK(BmCdsParSpread)->Unit(benchmark::kMicrosecond);

void BmBootstrapCreditCurve(benchmark::State& state) {
  const std::array<double, 5> maturities{1, 3, 5, 7, 10};
  const std::array<double, 5> spreads{0.006, 0.0075, 0.009, 0.0102, 0.0118};
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        credit::BootstrapCreditCurve(maturities, spreads, 4, 0.4, kDiscount));
  }
}
BENCHMARK(BmBootstrapCreditCurve)->Unit(benchmark::kMicrosecond);

void BmTrancheSpread(benchmark::State& state) {
  const credit::Tranche mezzanine{.attachment = 0.03, .detachment = 0.06};
  const credit::CdoPool pool{.names = static_cast<int>(state.range(0))};
  const credit::CdoTerms terms{};
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        credit::TrancheSpread(mezzanine, pool, 0.3, terms, kDiscount));
  }
}
BENCHMARK(BmTrancheSpread)->Arg(125)->Arg(500)->Unit(benchmark::kMillisecond);

void BmImpliedTrancheCorrelation(benchmark::State& state) {
  const credit::Tranche equity{.attachment = 0.0, .detachment = 0.03};
  const credit::CdoPool pool{};
  const credit::CdoTerms terms{};
  const double spread =
      *credit::TrancheSpread(equity, pool, 0.2, terms, kDiscount);
  for (auto _ : state) {
    benchmark::DoNotOptimize(credit::ImpliedTrancheCorrelation(
        equity, pool, spread, terms, kDiscount));
  }
}
BENCHMARK(BmImpliedTrancheCorrelation)->Unit(benchmark::kMillisecond);

void BmNthToDefault(benchmark::State& state) {
  const credit::BasketTerms terms{};
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        credit::NthToDefaultSpread(terms, 0.05, 100'000, 1));
  }
}
BENCHMARK(BmNthToDefault)->Unit(benchmark::kMillisecond);

void BmForwardExposure(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(credit::SimulateForwardExposure(
        100.0, 100.0, 0.05, 0.3, 5.0, 60, 10'000, 1));
  }
}
BENCHMARK(BmForwardExposure)->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// Rates: curve bootstrap, caps, bucket DV01, Black-Karasinski, MBS, and the
// Hull-White calibration (Ch 4, 29, 32, 33).

std::vector<curves::SwapQuote> SwapQuotes() {
  std::vector<curves::SwapQuote> quotes;
  for (int y = 1; y <= 30; ++y) {
    quotes.push_back({.maturity = static_cast<double>(y),
                      .par_rate = 0.03 + 0.001 * std::sqrt(y),
                      .frequency = 2});
  }
  return quotes;
}

void BmBootstrapSwaps(benchmark::State& state) {
  const auto quotes = SwapQuotes();
  for (auto _ : state) {
    benchmark::DoNotOptimize(curves::BootstrapFromSwaps(quotes));
  }
}
BENCHMARK(BmBootstrapSwaps)->Unit(benchmark::kMicrosecond);

void BmCapBucketDv01(benchmark::State& state) {
  const auto curve = *curves::BootstrapFromSwaps(SwapQuotes());
  const pricers::CurvePricer cap = [](const curves::ZeroCurve& c) {
    return *pricers::CapFloorPrice(OptionType::kCall, c,
                                   {.strike = 0.035, .maturity = 10.0}, 0.2);
  };
  for (auto _ : state) {
    benchmark::DoNotOptimize(pricers::BucketDv01(cap, curve));
  }
}
BENCHMARK(BmCapBucketDv01)->Unit(benchmark::kMicrosecond);

void BmBlackKarasinskiBermudan(benchmark::State& state) {
  const auto curve = curves::ZeroCurve::Flat(0.04);
  const int steps = static_cast<int>(state.range(0));
  std::vector<int> exercise;
  for (int e = steps / 10; e < steps; e += steps / 10) exercise.push_back(e);
  for (auto _ : state) {
    const auto tree = pricers::ShortRateTree::Build(
        curve, 0.1, 0.2, 10.0 / steps, steps,
        pricers::ShortRateModel::kBlackKarasinski);
    if (!tree) {
      state.SkipWithError("tree construction failed");
      break;
    }
    benchmark::DoNotOptimize(pricers::TreeSwaption(
        *tree, OptionType::kPut, 0.04, steps / 10, steps, exercise));
  }
}
BENCHMARK(BmBlackKarasinskiBermudan)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMillisecond);

void BmMbsMonteCarlo(benchmark::State& state) {
  const auto rates =
      *models::HullWhite::Create(curves::ZeroCurve::Flat(0.04), 0.1, 0.01);
  const pricers::MortgagePool pool{.balance = 100.0,
                                   .mortgage_rate = 0.06,
                                   .pass_through_rate = 0.055,
                                   .months = 360};
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        pricers::MbsMonteCarlo(pool, rates, {}, {.paths = 5'000}));
  }
}
BENCHMARK(BmMbsMonteCarlo)->Unit(benchmark::kMillisecond);

void BmCalibrateHullWhite(benchmark::State& state) {
  const auto curve = curves::ZeroCurve::Flat(0.05);
  const auto truth = *models::HullWhite::Create(curve, 0.08, 0.012);
  std::vector<models::CapletQuote> quotes;
  for (int i = 1; i <= 20; ++i) {
    const double reset = 0.5 * i;
    models::CapletQuote q{
        .reset = reset, .accrual = 0.5, .strike = 0.05, .market_price = 0.0};
    q.market_price =
        truth.Caplet(OptionType::kCall, q.reset, q.accrual, q.strike, 1.0);
    quotes.push_back(q);
  }
  for (auto _ : state) {
    benchmark::DoNotOptimize(models::CalibrateHullWhite(curve, quotes));
  }
}
BENCHMARK(BmCalibrateHullWhite)->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// Commodities and real options (Ch 35-36).

std::vector<double> SeasonalFutures(int steps, double dt) {
  std::vector<double> futures;
  for (int i = 1; i <= steps; ++i) {
    futures.push_back(20.0 * std::exp(0.02 * i * dt) *
                      (1.0 + 0.1 * std::sin(2.0 * M_PI * i * dt)));
  }
  return futures;
}

void BmCommodityTreeAmerican(benchmark::State& state) {
  const int steps = static_cast<int>(state.range(0));
  const double dt = 2.0 / steps;
  const auto futures = SeasonalFutures(steps, dt);
  for (auto _ : state) {
    const auto tree =
        pricers::CommodityTree::Build(20.0, futures, 0.5, 0.3, dt);
    if (!tree) {
      state.SkipWithError("tree construction failed");
      break;
    }
    benchmark::DoNotOptimize(pricers::CommodityTreeOption(
        *tree, OptionType::kPut, Exercise::kAmerican, 20.0, steps, 0.03));
  }
}
BENCHMARK(BmCommodityTreeAmerican)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMillisecond);

void BmExtractionProject(benchmark::State& state) {
  const int steps = static_cast<int>(state.range(0));
  const double dt = 10.0 / steps;
  const auto tree = *pricers::CommodityTree::Build(
      20.0, SeasonalFutures(steps, dt), 0.5, 0.3, dt);
  const pricers::ExtractionProject project{.production = 2.0 * dt,
                                           .variable_cost = 15.0,
                                           .fixed_cost = 5.0 * dt,
                                           .rate = 0.05,
                                           .abandon_value = 0.0,
                                           .expand_factor = 1.2,
                                           .expand_cost = 2.0};
  for (auto _ : state) {
    benchmark::DoNotOptimize(pricers::ValueExtractionProject(tree, project));
  }
}
BENCHMARK(BmExtractionProject)
    ->Arg(100)
    ->Arg(1000)
    ->Unit(benchmark::kMillisecond);

void BmSpikeMonteCarlo(benchmark::State& state) {
  const models::SpikeModel model{.log_level = std::log(30.0),
                                 .a = 5.0,
                                 .sigma = 0.6,
                                 .intensity = 6.0,
                                 .jump_mean = 0.5,
                                 .jump_vol = 0.3};
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        models::SpikeMonteCarlo(model, 0.0, 1.0, 365, 10'000, 1));
  }
}
BENCHMARK(BmSpikeMonteCarlo)->Unit(benchmark::kMillisecond);

void BmProjectWithOptions(benchmark::State& state) {
  const pricers::ProjectLattice lattice{
      .value = 100.0,
      .volatility = 0.3,
      .rate = 0.05,
      .payout = 0.02,
      .maturity = 5.0,
      .steps = static_cast<int>(state.range(0))};
  const std::array<pricers::ProjectOption, 3> options{
      pricers::Abandon{.salvage = 80.0},
      pricers::Expand{.factor = 1.3, .cost = 20.0},
      pricers::Contract{.factor = 0.7, .saving = 25.0}};
  for (auto _ : state) {
    benchmark::DoNotOptimize(pricers::ProjectWithOptions(lattice, options));
  }
}
BENCHMARK(BmProjectWithOptions)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMillisecond);

void BmValueBusiness(benchmark::State& state) {
  const pricers::BusinessSpec spec{.revenue = 50.0,
                                   .growth = 0.05,
                                   .volatility = 0.3,
                                   .variable_cost = 0.7,
                                   .fixed_cost = 12.0,
                                   .cash = 5.0,
                                   .rate = 0.04,
                                   .years = 10.0,
                                   .steps_per_year = 4,
                                   .terminal_multiple = 8.0};
  for (auto _ : state) {
    benchmark::DoNotOptimize(pricers::ValueBusiness(spec, 100'000, 1));
  }
}
BENCHMARK(BmValueBusiness)->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// Market risk (Ch 22-23).

std::vector<double> Returns(int n) {
  std::mt19937_64 gen(7);
  std::normal_distribution<double> z;
  std::vector<double> r;
  double v = 1e-4;
  for (int i = 0; i < n; ++i) {
    const double u = std::sqrt(v) * z(gen);
    r.push_back(u);
    v = 2e-6 + 0.1 * u * u + 0.88 * v;
  }
  return r;
}

void BmHistoricalVar(benchmark::State& state) {
  const auto losses = Returns(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    benchmark::DoNotOptimize(risk::HistoricalVar(losses, 0.99));
    benchmark::DoNotOptimize(risk::HistoricalExpectedShortfall(losses, 0.99));
  }
}
BENCHMARK(BmHistoricalVar)
    ->Arg(500)
    ->Arg(100'000)
    ->Unit(benchmark::kMicrosecond);

void BmMonteCarloVar(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  math::Matrix cov(static_cast<std::size_t>(n),
                   std::vector<double>(static_cast<std::size_t>(n)));
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      cov[i][j] = 1e-4 * (i == j ? 1.0 : 0.3);
    }
  }
  const risk::PnlFunction pnl = [](std::span<const double> dx) {
    double v = 0.0;
    for (double x : dx) v += 1e6 * x + 5e7 * x * x;
    return v;
  };
  for (auto _ : state) {
    benchmark::DoNotOptimize(risk::MonteCarloVar(pnl, cov, 0.99, 100'000, 1));
  }
}
BENCHMARK(BmMonteCarloVar)->Arg(10)->Arg(50)->Unit(benchmark::kMillisecond);

void BmPrincipalComponents(benchmark::State& state) {
  const int vars = static_cast<int>(state.range(0));
  std::mt19937_64 gen(3);
  std::normal_distribution<double> z;
  math::Matrix obs(1000, std::vector<double>(static_cast<std::size_t>(vars)));
  for (auto& row : obs) {
    const double level = z(gen);
    const double slope = z(gen);
    for (int j = 0; j < vars; ++j) {
      row[j] = level + slope * j / vars + 0.1 * z(gen);
    }
  }
  for (auto _ : state) {
    benchmark::DoNotOptimize(risk::PrincipalComponents(obs));
  }
}
BENCHMARK(BmPrincipalComponents)
    ->Arg(10)
    ->Arg(50)
    ->Unit(benchmark::kMillisecond);

void BmFitGarch(benchmark::State& state) {
  const auto r = Returns(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    benchmark::DoNotOptimize(stats::FitGarch11(r));
  }
}
BENCHMARK(BmFitGarch)->Arg(1000)->Arg(5000)->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// Greeks (Ch 19): analytic against bumping a 500-step tree.

void BmBsmGreeks(benchmark::State& state) {
  const greeks::OptionInputs in{
      .spot = 49, .rate = 0.05, .volatility = 0.2, .maturity = 0.3846};
  for (auto _ : state) {
    benchmark::DoNotOptimize(greeks::BsmGreeks(OptionType::kCall, 50, in));
  }
}
BENCHMARK(BmBsmGreeks);

void BmBumpGreeksTree(benchmark::State& state) {
  const greeks::OptionInputs in{
      .spot = 49, .rate = 0.05, .volatility = 0.2, .maturity = 0.3846};
  auto tree = [](const greeks::OptionInputs& x) {
    const VanillaOption option{.option_type = OptionType::kPut,
                               .exercise = Exercise::kAmerican,
                               .strike = 50.0,
                               .maturity = x.maturity};
    return pricers::PriceBinomial<pricers::CoxRossRubinstein>(
               option,
               {.spot = x.spot,
                .rate = x.rate,
                .yield = x.yield,
                .volatility = x.volatility},
               500)
        ->price;
  };
  for (auto _ : state) {
    benchmark::DoNotOptimize(greeks::BumpGreeks(tree, in));
  }
}
BENCHMARK(BmBumpGreeksTree)->Unit(benchmark::kMillisecond);

}  // namespace
}  // namespace ofod
