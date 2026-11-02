// Copyright 2026 Denis Zhereshchin
//
// Google Benchmark suite for the Ch 21 engines and the Ch 26-27 models. Run
// from the profile preset:
//   ./build/profile/cpp/bench/ofod_bench --benchmark_out=bench.json
//       --benchmark_out_format=json

#include <benchmark/benchmark.h>

#include <algorithm>
#include <span>

#include "ofod/pricers/american_mc.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/exotics.h"
#include "ofod/pricers/finite_difference.h"
#include "ofod/pricers/model_pricers.h"
#include "ofod/pricers/monte_carlo.h"

namespace ofod::pricers {
namespace {

using instruments::Exercise;
using instruments::OptionType;
using instruments::VanillaOption;

const VanillaOption kPut{.option_type = OptionType::kPut,
                         .exercise = Exercise::kAmerican,
                         .strike = 50.0,
                         .maturity = 5.0 / 12.0};
const VanillaOption kCall{.strike = 40.0, .maturity = 0.5};
const TreeMarket kMarket{.spot = 50.0, .rate = 0.10, .volatility = 0.40};

void BmBsm(benchmark::State& state) {
  double s = 42.0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        BsmPrice(OptionType::kCall, s, 40, 0.1, 0.0, 0.2, 0.5));
    s += 1e-9;
  }
}
BENCHMARK(BmBsm);

void BmImpliedVol(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        ImpliedVolatility(OptionType::kCall, 1.875, 21, 20, 0.1, 0.0, 0.25));
  }
}
BENCHMARK(BmImpliedVol);

void BmCrrAmerican(benchmark::State& state) {
  const int steps = static_cast<int>(state.range(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        PriceBinomial<CoxRossRubinstein>(kPut, kMarket, steps));
  }
  state.SetComplexityN(state.range(0));
}
BENCHMARK(BmCrrAmerican)->RangeMultiplier(4)->Range(64, 4096)->Complexity();

void BmTrinomialAmerican(benchmark::State& state) {
  const int steps = static_cast<int>(state.range(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(PriceTrinomial(kPut, kMarket, steps));
  }
  state.SetComplexityN(state.range(0));
}
BENCHMARK(BmTrinomialAmerican)
    ->RangeMultiplier(4)
    ->Range(64, 4096)
    ->Complexity();

void BmFiniteDifference(benchmark::State& state) {
  const auto scheme = static_cast<FdScheme>(state.range(0));
  const FdGrid grid{.s_max = 200, .price_steps = 400, .time_steps = 400};
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        PriceFiniteDifference(kPut, kMarket, grid, scheme));
  }
}
BENCHMARK(BmFiniteDifference)
    ->Arg(static_cast<int>(FdScheme::kExplicit))
    ->Arg(static_cast<int>(FdScheme::kImplicit))
    ->Arg(static_cast<int>(FdScheme::kCrankNicolson));

void BmMonteCarloEuropean(benchmark::State& state) {
  const auto method = static_cast<VarianceReduction>(state.range(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(MonteCarloEuropean(
        kCall, kMarket, {.paths = 100'000, .method = method}));
  }
}
BENCHMARK(BmMonteCarloEuropean)
    ->DenseRange(0, 6)
    ->Unit(benchmark::kMillisecond);

void BmPathSimulator(benchmark::State& state) {
  const int threads = static_cast<int>(state.range(0));
  auto asian = [](std::span<const double> path) {
    double sum = 0.0;
    for (double s : path.subspan(1)) sum += s;
    return std::max(sum / static_cast<double>(path.size() - 1) - 40.0, 0.0);
  };
  for (auto _ : state) {
    benchmark::DoNotOptimize(SimulateGbmPaths(
        kMarket, 1.0, 52, asian, {.paths = 100'000, .threads = threads}));
  }
}
BENCHMARK(BmPathSimulator)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

// M9: Heston semi-analytic and Monte Carlo, Longstaff-Schwartz, exotics.
const HestonParams kHeston{
    .v0 = 0.04, .kappa = 2.0, .theta = 0.04, .sigma = 0.3, .rho = -0.7};
const TreeMarket kHestonMarket{.spot = 100.0, .rate = 0.03};

void BmHestonPrice(benchmark::State& state) {
  const double t = static_cast<double>(state.range(0)) / 12.0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        HestonPrice(OptionType::kCall, kHestonMarket, 100.0, t, kHeston));
  }
}
BENCHMARK(BmHestonPrice)
    ->Arg(1)
    ->Arg(12)
    ->Arg(60)
    ->Unit(benchmark::kMicrosecond);

void BmHestonMonteCarlo(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(HestonMonteCarlo(
        OptionType::kCall, kHestonMarket, 100.0, 1.0, kHeston, 100, 10'000, 1));
  }
}
BENCHMARK(BmHestonMonteCarlo)->Unit(benchmark::kMillisecond);

void BmLongstaffSchwartz(benchmark::State& state) {
  const TreeMarket market{.spot = 36.0, .rate = 0.06, .volatility = 0.2};
  const int paths = static_cast<int>(state.range(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(LongstaffSchwartz(OptionType::kPut, market, 40.0,
                                               1.0, {.paths = paths}));
  }
}
BENCHMARK(BmLongstaffSchwartz)
    ->Arg(10'000)
    ->Arg(100'000)
    ->Unit(benchmark::kMillisecond);

void BmExoticClosedForms(benchmark::State& state) {
  const TreeMarket market{
      .spot = 100.0, .rate = 0.08, .yield = 0.04, .volatility = 0.25};
  for (auto _ : state) {
    benchmark::DoNotOptimize(BarrierOptionPrice(
        BarrierType::kDownAndOut, OptionType::kCall, market, 100.0, 95.0, 0.5));
    benchmark::DoNotOptimize(CompoundOptionPrice(
        CompoundType::kCallOnCall, market, 3.0, 0.25, 100.0, 1.0));
  }
}
BENCHMARK(BmExoticClosedForms)->Unit(benchmark::kMicrosecond);

}  // namespace
}  // namespace ofod::pricers
