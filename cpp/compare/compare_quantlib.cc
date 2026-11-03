// Copyright 2026 Denis Zhereshchin
//
// Google Benchmark comparison of ofod and QuantLib on the same inputs,
// C++ against C++. Each pair of benchmarks prices one instrument with
// matched settings and reports the price as the "value" counter, so the
// JSON output carries accuracy next to time. Build with
// -DOFOD_BUILD_COMPARE=ON (needs QuantLib) and run:
//   ./build/<preset>/cpp/compare/ofod_compare_quantlib
//       --benchmark_out=compare.json --benchmark_out_format=json
// docs/COMPARISON.md has the results and the reference values.

#include <benchmark/benchmark.h>

#include <cmath>
#include <memory>
#include <optional>
#include <ql/quantlib.hpp>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/pricers/american_mc.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/finite_difference.h"
#include "ofod/pricers/model_pricers.h"
#include "ofod/pricers/monte_carlo.h"
#include "ofod/pricers/short_rate_tree.h"

namespace ofod::compare {
namespace {

namespace ql = QuantLib;
using instruments::Exercise;
using instruments::OptionType;
using instruments::VanillaOption;
using pricers::TreeMarket;

// QuantLib works with dates. 30/360 makes whole months exact fractions of
// a year, so T = 5/12, 0.5, 1 and 10 match ofod exactly.
const ql::Date kToday(15, ql::January, 2026);
const ql::Thirty360 kDayCount(ql::Thirty360::USA);

ql::Date InMonths(int months) {
  return kToday + ql::Period(months, ql::Months);
}

void SetToday() { ql::Settings::instance().evaluationDate() = kToday; }

ql::Handle<ql::YieldTermStructure> Flat(double rate) {
  return ql::Handle<ql::YieldTermStructure>(
      ql::ext::make_shared<ql::FlatForward>(kToday, rate, kDayCount,
                                            ql::Continuous));
}

ql::ext::shared_ptr<ql::GeneralizedBlackScholesProcess> Process(double spot,
                                                                double rate,
                                                                double vol) {
  return ql::ext::make_shared<ql::BlackScholesMertonProcess>(
      ql::Handle<ql::Quote>(ql::ext::make_shared<ql::SimpleQuote>(spot)),
      Flat(0.0), Flat(rate),
      ql::Handle<ql::BlackVolTermStructure>(
          ql::ext::make_shared<ql::BlackConstantVol>(kToday, ql::NullCalendar(),
                                                     vol, kDayCount)));
}

// Prices `option` with `engine` on every iteration. Setting the engine
// marks the instrument as needing recalculation.
void RunQuantLib(benchmark::State& state, ql::Instrument& option,
                 const ql::ext::shared_ptr<ql::PricingEngine>& engine) {
  for (auto _ : state) {
    option.setPricingEngine(engine);
    benchmark::DoNotOptimize(option.NPV());
  }
  option.setPricingEngine(engine);
  state.counters["value"] = option.NPV();
}

// Times `price` and reports one more call's result as the "value" counter.
// The value is taken outside the timed loop: reading a result that
// DoNotOptimize has touched gave garbage with GCC 13 at -O3.
double Value(double x) { return x; }
double Value(const std::optional<double>& x) { return x.value_or(NAN); }
double Value(const std::optional<pricers::TreeResult>& x) {
  return x ? x->price : NAN;
}
double Value(const std::optional<pricers::McResult>& x) {
  return x ? x->price : NAN;
}

template <typename F>
void Run(benchmark::State& state, F price) {
  for (auto _ : state) {
    auto result = price();
    benchmark::DoNotOptimize(result);
  }
  state.counters["value"] = Value(price());
}

// BSM call, S = 42, K = 40, r = 10%, sigma = 20%, T = 0.5.
void BmBsmOfod(benchmark::State& state) {
  Run(state, [&] {
    return pricers::BsmPrice(OptionType::kCall, 42.0, 40.0, 0.1, 0.0, 0.2, 0.5);
  });
}
BENCHMARK(BmBsmOfod);

void BmBsmQuantLib(benchmark::State& state) {
  const double df = std::exp(-0.1 * 0.5);
  const double forward = 42.0 / df;
  const double std_dev = 0.2 * std::sqrt(0.5);
  Run(state, [&] {
    return ql::blackFormula(ql::Option::Call, 40.0, forward, std_dev, df);
  });
}
BENCHMARK(BmBsmQuantLib);

// The same price through QuantLib's instrument and engine objects.
void BmBsmQuantLibEngine(benchmark::State& state) {
  SetToday();
  ql::VanillaOption option(
      ql::ext::make_shared<ql::PlainVanillaPayoff>(ql::Option::Call, 40.0),
      ql::ext::make_shared<ql::EuropeanExercise>(InMonths(6)));
  RunQuantLib(state, option,
              ql::ext::make_shared<ql::AnalyticEuropeanEngine>(
                  Process(42.0, 0.1, 0.2)));
}
BENCHMARK(BmBsmQuantLibEngine);

// Implied volatility of a call at 1.875, S = 21, K = 20, r = 10%, T = 0.25.
void BmImpliedVolOfod(benchmark::State& state) {
  Run(state, [&] {
    return pricers::ImpliedVolatility(OptionType::kCall, 1.875, 21.0, 20.0, 0.1,
                                      0.0, 0.25);
  });
}
BENCHMARK(BmImpliedVolOfod);

void BmImpliedVolQuantLib(benchmark::State& state) {
  const double df = std::exp(-0.1 * 0.25);
  const double forward = 21.0 / df;
  // Accuracy 1e-12 on the standard deviation, rather than the default
  // 1e-6, so both libraries return the root to about the same precision.
  Run(state, [&] {
    return ql::blackFormulaImpliedStdDev(ql::Option::Call, 20.0, forward, 1.875,
                                         df, 0.0, ql::Null<double>(), 1e-12) /
           0.5;
  });
}
BENCHMARK(BmImpliedVolQuantLib);

// American put, S = K = 50, r = 10%, sigma = 40%, T = 5/12.
const VanillaOption kPut{.option_type = OptionType::kPut,
                         .exercise = Exercise::kAmerican,
                         .strike = 50.0,
                         .maturity = 5.0 / 12.0};
const TreeMarket kMarket{.spot = 50.0, .rate = 0.10, .volatility = 0.40};

ql::VanillaOption QlAmericanPut(double strike, int months) {
  SetToday();
  return ql::VanillaOption(
      ql::ext::make_shared<ql::PlainVanillaPayoff>(ql::Option::Put, strike),
      ql::ext::make_shared<ql::AmericanExercise>(kToday, InMonths(months)));
}

void BmCrrOfod(benchmark::State& state) {
  const int steps = static_cast<int>(state.range(0));
  Run(state, [&] {
    return pricers::PriceBinomial<pricers::CoxRossRubinstein>(kPut, kMarket,
                                                              steps);
  });
}
BENCHMARK(BmCrrOfod)->Arg(100)->Arg(1000)->Unit(benchmark::kMicrosecond);

void BmCrrQuantLib(benchmark::State& state) {
  auto option = QlAmericanPut(50.0, 5);
  RunQuantLib(
      state, option,
      ql::ext::make_shared<ql::BinomialVanillaEngine<ql::CoxRossRubinstein>>(
          Process(50.0, 0.1, 0.4), static_cast<ql::Size>(state.range(0))));
}
BENCHMARK(BmCrrQuantLib)->Arg(100)->Arg(1000)->Unit(benchmark::kMicrosecond);

// American put by Crank-Nicolson, 200 price steps x 100 time steps.
void BmFdOfod(benchmark::State& state) {
  const auto solver = static_cast<pricers::AmericanSolver>(state.range(0));
  const pricers::FdGrid grid{
      .s_max = 200, .price_steps = 200, .time_steps = 100};
  Run(state, [&] {
    return pricers::PriceFiniteDifference(
        kPut, kMarket, grid, pricers::FdScheme::kCrankNicolson, solver);
  });
}
BENCHMARK(BmFdOfod)
    ->Arg(static_cast<int>(pricers::AmericanSolver::kBrennanSchwartz))
    ->Arg(static_cast<int>(pricers::AmericanSolver::kProjection))
    ->Arg(static_cast<int>(pricers::AmericanSolver::kPsor))
    ->Unit(benchmark::kMicrosecond);

void BmFdQuantLib(benchmark::State& state) {
  auto option = QlAmericanPut(50.0, 5);
  RunQuantLib(state, option,
              ql::MakeFdBlackScholesVanillaEngine(Process(50.0, 0.1, 0.4))
                  .withTGrid(100)
                  .withXGrid(200)
                  .withDampingSteps(0)
                  .withFdmSchemeDesc(ql::FdmSchemeDesc::CrankNicolson()));
}
BENCHMARK(BmFdQuantLib)->Unit(benchmark::kMicrosecond);

// European call by Monte Carlo, 100,000 draws, S = 50, K = 40, T = 0.5.
void BmMcOfod(benchmark::State& state) {
  const VanillaOption call{.strike = 40.0, .maturity = 0.5};
  Run(state, [&] {
    return pricers::MonteCarloEuropean(call, kMarket, {.paths = 100'000});
  });
}
BENCHMARK(BmMcOfod)->Unit(benchmark::kMillisecond);

void BmMcOfodZiggurat(benchmark::State& state) {
  const VanillaOption call{.strike = 40.0, .maturity = 0.5};
  Run(state, [&] {
    return pricers::MonteCarloEuropean(
        call, kMarket, {.paths = 100'000, .rng = pricers::Rng::kZiggurat});
  });
}
BENCHMARK(BmMcOfodZiggurat)->Unit(benchmark::kMillisecond);

void BmMcQuantLib(benchmark::State& state) {
  SetToday();
  ql::VanillaOption option(
      ql::ext::make_shared<ql::PlainVanillaPayoff>(ql::Option::Call, 40.0),
      ql::ext::make_shared<ql::EuropeanExercise>(InMonths(6)));
  RunQuantLib(
      state, option,
      ql::MakeMCEuropeanEngine<ql::PseudoRandom>(Process(50.0, 0.1, 0.4))
          .withSteps(1)
          .withSamples(100'000)
          .withSeed(42));
}
BENCHMARK(BmMcQuantLib)->Unit(benchmark::kMillisecond);

// Heston call, S = K = 100, r = 3%, T = 1.
const pricers::HestonParams kHeston{
    .v0 = 0.04, .kappa = 2.0, .theta = 0.04, .sigma = 0.3, .rho = -0.7};

void BmHestonOfod(benchmark::State& state) {
  const TreeMarket market{.spot = 100.0, .rate = 0.03};
  Run(state, [&] {
    return pricers::HestonPrice(OptionType::kCall, market, 100.0, 1.0, kHeston);
  });
}
BENCHMARK(BmHestonOfod)->Unit(benchmark::kMicrosecond);

ql::ext::shared_ptr<ql::HestonModel> QlHestonModel() {
  SetToday();
  return ql::ext::make_shared<ql::HestonModel>(
      ql::ext::make_shared<ql::HestonProcess>(
          Flat(0.03), Flat(0.0),
          ql::Handle<ql::Quote>(ql::ext::make_shared<ql::SimpleQuote>(100.0)),
          kHeston.v0, kHeston.kappa, kHeston.theta, kHeston.sigma,
          kHeston.rho));
}

ql::VanillaOption QlHestonCall() {
  return ql::VanillaOption(
      ql::ext::make_shared<ql::PlainVanillaPayoff>(ql::Option::Call, 100.0),
      ql::ext::make_shared<ql::EuropeanExercise>(InMonths(12)));
}

// 0: Gauss-Laguerre (144 points, the default); 1: adaptive Gauss-Lobatto
// at 1e-8 relative tolerance; 2: COS method.
void BmHestonQuantLib(benchmark::State& state) {
  const auto model = QlHestonModel();
  auto option = QlHestonCall();
  ql::ext::shared_ptr<ql::PricingEngine> engine;
  switch (state.range(0)) {
    case 0:
      engine = ql::ext::make_shared<ql::AnalyticHestonEngine>(model);
      break;
    case 1:
      engine =
          ql::ext::make_shared<ql::AnalyticHestonEngine>(model, 1e-8, 100'000);
      break;
    default:
      engine = ql::ext::make_shared<ql::COSHestonEngine>(model);
  }
  RunQuantLib(state, option, engine);
}
BENCHMARK(BmHestonQuantLib)->DenseRange(0, 2)->Unit(benchmark::kMicrosecond);

// Longstaff-Schwartz put, S = 36, K = 40, r = 6%, sigma = 20%, T = 1,
// 10,000 paths and 50 exercise dates.
void BmLsmOfod(benchmark::State& state) {
  const TreeMarket market{.spot = 36.0, .rate = 0.06, .volatility = 0.2};
  Run(state, [&] {
    return pricers::LongstaffSchwartz(OptionType::kPut, market, 40.0, 1.0,
                                      {.paths = 10'000});
  });
}
BENCHMARK(BmLsmOfod)->Unit(benchmark::kMillisecond);

// QuantLib runs separate calibration paths before the pricing paths; both
// are 10,000 here.
void BmLsmQuantLib(benchmark::State& state) {
  auto option = QlAmericanPut(40.0, 12);
  RunQuantLib(
      state, option,
      ql::MakeMCAmericanEngine<ql::PseudoRandom>(Process(36.0, 0.06, 0.2))
          .withSteps(50)
          .withSamples(10'000)
          .withCalibrationSamples(10'000)
          .withPolynomialOrder(2)
          .withBasisSystem(ql::LsmBasisSystem::Monomial)
          .withSeed(42));
}
BENCHMARK(BmLsmQuantLib)->Unit(benchmark::kMillisecond);

// Receiver Bermudan swaption in Hull-White (a = 0.1, sigma = 1%) on a flat
// 4% curve: 10-year annual swap at 4%, exercisable on years 1 to 9.
void BmHullWhiteOfod(benchmark::State& state) {
  const auto curve = curves::ZeroCurve::Flat(0.04);
  const int steps = static_cast<int>(state.range(0));
  std::vector<int> exercise;
  for (int e = steps / 10; e < steps; e += steps / 10) exercise.push_back(e);
  // The tree is built inside the timed call, as QuantLib's engine does.
  Run(state, [&] {
    const auto tree =
        pricers::ShortRateTree::Build(curve, 0.1, 0.01, 10.0 / steps, steps);
    return pricers::TreeSwaption(*tree, OptionType::kPut, 0.04, steps / 10,
                                 steps, exercise);
  });
}
BENCHMARK(BmHullWhiteOfod)->Arg(500)->Unit(benchmark::kMillisecond);

void BmHullWhiteQuantLib(benchmark::State& state) {
  SetToday();
  const auto curve = Flat(0.04);
  const auto index = ql::ext::make_shared<ql::IborIndex>(
      "Annual", ql::Period(1, ql::Years), 0, ql::USDCurrency(),
      ql::NullCalendar(), ql::Unadjusted, false, kDayCount, curve);
  const ql::Schedule schedule(
      kToday, InMonths(120), ql::Period(1, ql::Years), ql::NullCalendar(),
      ql::Unadjusted, ql::Unadjusted, ql::DateGeneration::Forward, false);
  const auto swap = ql::ext::make_shared<ql::VanillaSwap>(
      ql::Swap::Receiver, 1.0, schedule, 0.04, kDayCount, schedule, index, 0.0,
      kDayCount);
  std::vector<ql::Date> dates;
  for (int y = 1; y < 10; ++y) dates.push_back(InMonths(12 * y));
  ql::Swaption swaption(swap,
                        ql::ext::make_shared<ql::BermudanExercise>(dates));
  const auto model = ql::ext::make_shared<ql::HullWhite>(curve, 0.1, 0.01);
  RunQuantLib(state, swaption,
              ql::ext::make_shared<ql::TreeSwaptionEngine>(
                  model, static_cast<ql::Size>(state.range(0))));
}
BENCHMARK(BmHullWhiteQuantLib)->Arg(500)->Unit(benchmark::kMillisecond);

}  // namespace
}  // namespace ofod::compare
