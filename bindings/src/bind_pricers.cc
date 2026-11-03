// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <span>
#include <vector>

#include "bindings.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/bond_pricer.h"
#include "ofod/pricers/bounds.h"
#include "ofod/pricers/eso.h"
#include "ofod/pricers/finite_difference.h"
#include "ofod/pricers/forward_pricer.h"
#include "ofod/pricers/fra_pricer.h"
#include "ofod/pricers/monte_carlo.h"
#include "ofod/pricers/smile.h"
#include "ofod/pricers/swap_pricer.h"

namespace ofod::bindings {

namespace nb = nanobind;

namespace {

constexpr const char* kBadTree =
    "invalid tree: need steps >= 1, u > d, 0 <= p <= 1";

}  // namespace

void BindPricers(nb::module_& m) {
  using instruments::Bond;
  const Compounding continuous = Compounding::Continuous();

  m.def("bond_price", &pricers::BondPrice, nb::arg("bond"), nb::arg("curve"));
  m.def("bond_price_from_yield", &pricers::BondPriceFromYield, nb::arg("bond"),
        nb::arg("yield_"), nb::arg("c") = continuous);
  m.def(
      "bond_yield",
      [](const Bond& bond, double price, Compounding c) {
        const auto y = pricers::BondYield(bond, price, c);
        if (!y) throw nb::value_error("no yield reprices the bond");
        return *y;
      },
      nb::arg("bond"), nb::arg("price"), nb::arg("c") = continuous);
  m.def("macaulay_duration", &pricers::MacaulayDuration, nb::arg("bond"),
        nb::arg("yield_"), nb::arg("c") = continuous);
  m.def("modified_duration", &pricers::ModifiedDuration, nb::arg("bond"),
        nb::arg("yield_"), nb::arg("c"));
  m.def("convexity", &pricers::Convexity, nb::arg("bond"), nb::arg("yield_"));
  m.def("par_yield", &pricers::ParYield, nb::arg("maturity"),
        nb::arg("frequency"), nb::arg("curve"));

  m.def("simple_forward_rate", &pricers::SimpleForwardRate, nb::arg("curve"),
        nb::arg("t1"), nb::arg("t2"));
  m.def("fra_value", &pricers::FraValue, nb::arg("fra"), nb::arg("curve"),
        nb::arg("side"));

  m.def("forward_price", &pricers::ForwardPrice, nb::arg("spot"),
        nb::arg("rate"), nb::arg("t"));
  m.def("forward_price_with_income", &pricers::ForwardPriceWithIncome,
        nb::arg("spot"), nb::arg("income_pv"), nb::arg("rate"), nb::arg("t"));
  m.def("forward_price_with_yield", &pricers::ForwardPriceWithYield,
        nb::arg("spot"), nb::arg("rate"), nb::arg("yield_"), nb::arg("t"));
  m.def("forward_price_cost_of_carry", &pricers::ForwardPriceCostOfCarry,
        nb::arg("spot"), nb::arg("rate"), nb::arg("storage_yield"),
        nb::arg("convenience_yield"), nb::arg("t"));
  m.def(
      "present_value",
      [](const std::vector<instruments::CashFlow>& flows,
         const curves::ZeroCurve& curve) {
        return pricers::PresentValue(flows, curve);
      },
      nb::arg("flows"), nb::arg("curve"));
  m.def("forward_contract_value", &pricers::ForwardContractValue,
        nb::arg("forward_price_"), nb::arg("delivery_price"), nb::arg("rate"),
        nb::arg("t"), nb::arg("side"));

  nb::enum_<pricers::ForwardArbitrage>(m, "ForwardArbitrage")
      .value("NONE", pricers::ForwardArbitrage::kNone)
      .value("CASH_AND_CARRY", pricers::ForwardArbitrage::kCashAndCarry)
      .value("REVERSE_CASH_AND_CARRY",
             pricers::ForwardArbitrage::kReverseCashAndCarry);
  m.def("detect_forward_arbitrage", &pricers::DetectForwardArbitrage,
        nb::arg("quoted_forward"), nb::arg("theoretical"),
        nb::arg("tolerance") = 1e-9);

  m.def("fixed_leg_bond_value", &pricers::FixedLegBondValue, nb::arg("s"),
        nb::arg("curve"));
  m.def("floating_leg_bond_value", &pricers::FloatingLegBondValue, nb::arg("s"),
        nb::arg("curve"));
  m.def("swap_value", &pricers::SwapValue, nb::arg("s"), nb::arg("curve"),
        nb::arg("side"));
  m.def("swap_value_as_fras", &pricers::SwapValueAsFras, nb::arg("s"),
        nb::arg("curve"), nb::arg("side"));
  m.def(
      "par_swap_rate",
      [](const std::vector<double>& payment_times, int frequency,
         const curves::ZeroCurve& curve) {
        return pricers::ParSwapRate(payment_times, frequency, curve);
      },
      nb::arg("payment_times"), nb::arg("frequency"), nb::arg("curve"));
  m.def("currency_swap_value", &pricers::CurrencySwapValue, nb::arg("s"),
        nb::arg("domestic"), nb::arg("foreign"), nb::arg("spot"),
        nb::arg("side"));
  m.def("comparative_advantage_gain", &pricers::ComparativeAdvantageGain,
        nb::arg("fixed_a"), nb::arg("fixed_b"), nb::arg("floating_spread_a"),
        nb::arg("floating_spread_b"));

  m.def("european_call_lower_bound", &pricers::EuropeanCallLowerBound,
        nb::arg("spot"), nb::arg("strike"), nb::arg("rate"), nb::arg("t"),
        nb::arg("dividends_pv") = 0.0);
  m.def("european_put_lower_bound", &pricers::EuropeanPutLowerBound,
        nb::arg("spot"), nb::arg("strike"), nb::arg("rate"), nb::arg("t"),
        nb::arg("dividends_pv") = 0.0);
  m.def("call_upper_bound", &pricers::CallUpperBound, nb::arg("spot"));
  m.def("european_put_upper_bound", &pricers::EuropeanPutUpperBound,
        nb::arg("strike"), nb::arg("rate"), nb::arg("t"));
  m.def("american_put_upper_bound", &pricers::AmericanPutUpperBound,
        nb::arg("strike"));
  m.def("put_from_parity", &pricers::PutFromParity, nb::arg("call"),
        nb::arg("spot"), nb::arg("strike"), nb::arg("rate"), nb::arg("t"),
        nb::arg("dividends_pv") = 0.0);
  m.def("call_from_parity", &pricers::CallFromParity, nb::arg("put"),
        nb::arg("spot"), nb::arg("strike"), nb::arg("rate"), nb::arg("t"),
        nb::arg("dividends_pv") = 0.0);
  nb::class_<pricers::Range>(m, "Range")
      .def(nb::init<double, double>(), nb::arg("lower"), nb::arg("upper"))
      .def_rw("lower", &pricers::Range::lower)
      .def_rw("upper", &pricers::Range::upper);
  m.def("american_call_minus_put", &pricers::AmericanCallMinusPut,
        nb::arg("spot"), nb::arg("strike"), nb::arg("rate"), nb::arg("t"),
        nb::arg("dividends_pv") = 0.0);
  m.def("call_early_exercise_may_be_optimal",
        &pricers::CallEarlyExerciseMayBeOptimal, nb::arg("dividend"),
        nb::arg("strike"), nb::arg("rate"), nb::arg("time_to_next"));

  using pricers::TreeMarket;
  using pricers::TreeResult;
  using pricers::TreeScheme;
  using pricers::TreeStep;
  nb::class_<TreeStep>(m, "TreeStep")
      .def(nb::init<double, double, double>(), nb::arg("up"), nb::arg("down"),
           nb::arg("probability"))
      .def_rw("up", &TreeStep::up)
      .def_rw("down", &TreeStep::down)
      .def_rw("probability", &TreeStep::probability);
  m.def("risk_neutral_probability", &pricers::RiskNeutralProbability,
        nb::arg("up"), nb::arg("down"), nb::arg("growth"));
  nb::enum_<TreeScheme>(m, "TreeScheme")
      .value("COX_ROSS_RUBINSTEIN", TreeScheme::kCoxRossRubinstein)
      .value("JARROW_RUDD", TreeScheme::kJarrowRudd)
      .value("TIAN", TreeScheme::kTian);
  m.def("scheme_step", &pricers::SchemeStep, nb::arg("scheme"),
        nb::arg("volatility"), nb::arg("drift"), nb::arg("dt"));
  nb::class_<TreeMarket>(m, "TreeMarket")
      .def(
          "__init__",
          [](TreeMarket* self, double spot, double rate, double yield,
             double volatility) {
            new (self) TreeMarket{spot, rate, yield, volatility};
          },
          nb::arg("spot") = 0.0, nb::arg("rate") = 0.0, nb::arg("yield_") = 0.0,
          nb::arg("volatility") = 0.0)
      .def_rw("spot", &TreeMarket::spot)
      .def_rw("rate", &TreeMarket::rate)
      .def_rw("yield_", &TreeMarket::yield)
      .def_rw("volatility", &TreeMarket::volatility);
  nb::class_<TreeResult>(m, "TreeResult")
      .def(nb::init<double, double>(), nb::arg("price"), nb::arg("delta"))
      .def_rw("price", &TreeResult::price)
      .def_rw("delta", &TreeResult::delta);
  m.def(
      "price_tree",
      [](const instruments::VanillaOption& option, double spot, double rate,
         int steps, const TreeStep& step) {
        const auto r = pricers::PriceTree(option, spot, rate, steps, step);
        if (!r) throw nb::value_error(kBadTree);
        return *r;
      },
      nb::arg("option"), nb::arg("spot"), nb::arg("rate"), nb::arg("steps"),
      nb::arg("step"));
  m.def(
      "price_binomial",
      [](const instruments::VanillaOption& option, const TreeMarket& market,
         int steps, TreeScheme scheme) {
        const auto r = pricers::PriceBinomial(option, market, steps, scheme);
        if (!r) throw nb::value_error(kBadTree);
        return *r;
      },
      nb::arg("option"), nb::arg("market"), nb::arg("steps"),
      nb::arg("scheme") = TreeScheme::kCoxRossRubinstein);

  using instruments::CashFlow;
  using instruments::OptionType;
  m.def("bsm_d1", &pricers::BsmD1, nb::arg("spot"), nb::arg("strike"),
        nb::arg("rate"), nb::arg("yield_"), nb::arg("volatility"),
        nb::arg("t"));
  m.def("bsm_d2", &pricers::BsmD2, nb::arg("spot"), nb::arg("strike"),
        nb::arg("rate"), nb::arg("yield_"), nb::arg("volatility"),
        nb::arg("t"));
  m.def("bsm_price", &pricers::BsmPrice, nb::arg("option_type"),
        nb::arg("spot"), nb::arg("strike"), nb::arg("rate"), nb::arg("yield_"),
        nb::arg("volatility"), nb::arg("t"));
  m.def("garman_kohlhagen_price", &pricers::GarmanKohlhagenPrice,
        nb::arg("option_type"), nb::arg("spot"), nb::arg("strike"),
        nb::arg("rate"), nb::arg("foreign_rate"), nb::arg("volatility"),
        nb::arg("t"));
  m.def("black76_price", &pricers::Black76Price, nb::arg("option_type"),
        nb::arg("futures"), nb::arg("strike"), nb::arg("rate"),
        nb::arg("volatility"), nb::arg("t"));
  m.def("futures_style_option_price", &pricers::FuturesStyleOptionPrice,
        nb::arg("option_type"), nb::arg("futures"), nb::arg("strike"),
        nb::arg("volatility"), nb::arg("t"));
  m.def("futures_put_from_parity", &pricers::FuturesPutFromParity,
        nb::arg("call"), nb::arg("futures"), nb::arg("strike"), nb::arg("rate"),
        nb::arg("t"));
  m.def(
      "implied_volatility",
      [](OptionType type, double price, double spot, double strike, double rate,
         double yield, double t) {
        const auto vol = pricers::ImpliedVolatility(type, price, spot, strike,
                                                    rate, yield, t);
        if (!vol) {
          throw nb::value_error(
              "price outside the no-arbitrage bounds or not reproducible");
        }
        return *vol;
      },
      nb::arg("option_type"), nb::arg("price"), nb::arg("spot"),
      nb::arg("strike"), nb::arg("rate"), nb::arg("yield_"), nb::arg("t"));
  m.def(
      "bsm_price_with_dividends",
      [](OptionType type, double spot, double strike, double rate,
         double volatility, double t, const std::vector<CashFlow>& dividends) {
        return pricers::BsmPriceWithDividends(type, spot, strike, rate,
                                              volatility, t, dividends);
      },
      nb::arg("option_type"), nb::arg("spot"), nb::arg("strike"),
      nb::arg("rate"), nb::arg("volatility"), nb::arg("t"),
      nb::arg("dividends"));
  m.def(
      "black_american_call_approximation",
      [](double spot, double strike, double rate, double volatility, double t,
         const std::vector<CashFlow>& dividends) {
        return pricers::BlackAmericanCallApproximation(
            spot, strike, rate, volatility, t, dividends);
      },
      nb::arg("spot"), nb::arg("strike"), nb::arg("rate"),
      nb::arg("volatility"), nb::arg("t"), nb::arg("dividends"));
  m.def("warrant_price", &pricers::WarrantPrice, nb::arg("spot"),
        nb::arg("strike"), nb::arg("rate"), nb::arg("volatility"), nb::arg("t"),
        nb::arg("shares"), nb::arg("warrants"));

  // Chapter 21: trees with dividends, control variate, trinomial.
  m.def(
      "price_binomial_with_dividends",
      [](const instruments::VanillaOption& option, const TreeMarket& market,
         int steps, const std::vector<CashFlow>& dividends) {
        const auto r = pricers::PriceBinomialWithDividends(option, market,
                                                           steps, dividends);
        if (!r) throw nb::value_error("need steps >= 1 and a zero yield");
        return *r;
      },
      nb::arg("option"), nb::arg("market"), nb::arg("steps"),
      nb::arg("dividends"));
  m.def(
      "control_variate_american_price",
      [](const instruments::VanillaOption& option, const TreeMarket& market,
         int steps) {
        const auto r =
            pricers::ControlVariateAmericanPrice(option, market, steps);
        if (!r) throw nb::value_error(kBadTree);
        return *r;
      },
      nb::arg("option"), nb::arg("market"), nb::arg("steps"));
  m.def(
      "price_trinomial",
      [](const instruments::VanillaOption& option, const TreeMarket& market,
         int steps) {
        const auto r = pricers::PriceTrinomial(option, market, steps);
        if (!r) {
          throw nb::value_error(
              "need steps >= 1, a positive volatility and non-negative "
              "probabilities");
        }
        return *r;
      },
      nb::arg("option"), nb::arg("market"), nb::arg("steps"));

  // Monte Carlo.
  using pricers::McOptions;
  using pricers::McResult;
  using pricers::VarianceReduction;
  nb::enum_<VarianceReduction>(m, "VarianceReduction")
      .value("NONE", VarianceReduction::kNone)
      .value("ANTITHETIC", VarianceReduction::kAntithetic)
      .value("CONTROL_VARIATE", VarianceReduction::kControlVariate)
      .value("IMPORTANCE_SAMPLING", VarianceReduction::kImportanceSampling)
      .value("STRATIFIED", VarianceReduction::kStratified)
      .value("MOMENT_MATCHING", VarianceReduction::kMomentMatching)
      .value("QUASI_RANDOM", VarianceReduction::kQuasiRandom);
  nb::enum_<pricers::Rng>(m, "Rng")
      .value("MERSENNE_TWISTER", pricers::Rng::kMersenneTwister)
      .value("ZIGGURAT", pricers::Rng::kZiggurat);
  nb::class_<McOptions>(m, "McOptions")
      .def(
          "__init__",
          [](McOptions* self, int paths, std::uint64_t seed,
             VarianceReduction method, int threads, pricers::Rng rng) {
            new (self) McOptions{paths, seed, method, threads, rng};
          },
          nb::arg("paths") = 100'000, nb::arg("seed") = 1,
          nb::arg("method") = VarianceReduction::kNone, nb::arg("threads") = 1,
          nb::arg("rng") = pricers::Rng::kMersenneTwister)
      .def_rw("paths", &McOptions::paths)
      .def_rw("seed", &McOptions::seed)
      .def_rw("method", &McOptions::method)
      .def_rw("threads", &McOptions::threads)
      .def_rw("rng", &McOptions::rng);
  nb::class_<McResult>(m, "McResult")
      .def(nb::init<double, double, int>(), nb::arg("price"),
           nb::arg("std_error"), nb::arg("paths"))
      .def_rw("price", &McResult::price)
      .def_rw("std_error", &McResult::std_error)
      .def_rw("paths", &McResult::paths);
  m.attr("CHUNK_PATHS") = pricers::kChunkPaths;
  m.def("mix_seed", &pricers::MixSeed, nb::arg("seed"), nb::arg("chunk"));
  m.def(
      "monte_carlo_european",
      [](const instruments::VanillaOption& option, const TreeMarket& market,
         const McOptions& options) {
        const auto r = pricers::MonteCarloEuropean(option, market, options);
        if (!r) {
          throw nb::value_error("need a European option and at least 32 paths");
        }
        return *r;
      },
      nb::arg("option"), nb::arg("market"), nb::arg("options"));
  m.def(
      "simulate_gbm_paths",
      [](const TreeMarket& market, double t, int steps, nb::callable payoff,
         McOptions options) {
        // The payoff is Python code, so it runs on this thread with the GIL;
        // results do not depend on the thread count anyway.
        options.threads = 1;
        auto wrapped = [&payoff](std::span<const double> path) {
          return nb::cast<double>(
              payoff(std::vector<double>(path.begin(), path.end())));
        };
        const auto r =
            pricers::SimulateGbmPaths(market, t, steps, wrapped, options);
        if (!r) {
          throw nb::value_error(
              "need steps >= 1, paths >= 2, threads >= 1 and NONE or "
              "ANTITHETIC");
        }
        return *r;
      },
      nb::arg("market"), nb::arg("t"), nb::arg("steps"), nb::arg("payoff"),
      nb::arg("options"));

  // Finite differences.
  using pricers::AmericanSolver;
  using pricers::FdGrid;
  using pricers::FdScheme;
  nb::enum_<FdScheme>(m, "FdScheme")
      .value("EXPLICIT", FdScheme::kExplicit)
      .value("IMPLICIT", FdScheme::kImplicit)
      .value("CRANK_NICOLSON", FdScheme::kCrankNicolson);
  nb::enum_<AmericanSolver>(m, "AmericanSolver")
      .value("PSOR", AmericanSolver::kPsor)
      .value("PROJECTION", AmericanSolver::kProjection)
      .value("BRENNAN_SCHWARTZ", AmericanSolver::kBrennanSchwartz);
  nb::class_<FdGrid>(m, "FdGrid")
      .def(
          "__init__",
          [](FdGrid* self, double s_min, double s_max, int price_steps,
             int time_steps, bool log_space) {
            new (self) FdGrid{s_min, s_max, price_steps, time_steps, log_space};
          },
          nb::arg("s_min") = 0.0, nb::arg("s_max") = 0.0,
          nb::arg("price_steps") = 100, nb::arg("time_steps") = 100,
          nb::arg("log_space") = false)
      .def_rw("s_min", &FdGrid::s_min)
      .def_rw("s_max", &FdGrid::s_max)
      .def_rw("price_steps", &FdGrid::price_steps)
      .def_rw("time_steps", &FdGrid::time_steps)
      .def_rw("log_space", &FdGrid::log_space);
  m.def(
      "price_finite_difference",
      [](const instruments::VanillaOption& option, const TreeMarket& market,
         const FdGrid& grid, FdScheme scheme, AmericanSolver american) {
        const auto v = pricers::PriceFiniteDifference(option, market, grid,
                                                      scheme, american);
        if (!v) throw nb::value_error("invalid finite difference grid");
        return *v;
      },
      nb::arg("option"), nb::arg("market"), nb::arg("grid"), nb::arg("scheme"),
      nb::arg("american") = AmericanSolver::kPsor);

  // Chapter 20: smiles.
  m.def(
      "strike_from_delta",
      [](OptionType type, double delta, double spot, double rate, double yield,
         double volatility, double t) {
        const auto k = pricers::StrikeFromDelta(type, delta, spot, rate, yield,
                                                volatility, t);
        if (!k) throw nb::value_error("delta outside the attainable range");
        return *k;
      },
      nb::arg("option_type"), nb::arg("delta"), nb::arg("spot"),
      nb::arg("rate"), nb::arg("yield_"), nb::arg("volatility"), nb::arg("t"));
  m.def("minimum_variance_delta", &pricers::MinimumVarianceDelta,
        nb::arg("bsm_delta"), nb::arg("bsm_vega"), nb::arg("spot"),
        nb::arg("t"), nb::arg("a"), nb::arg("b"), nb::arg("c"));
  m.def("implied_density", &pricers::ImpliedDensity, nb::arg("call_low"),
        nb::arg("call_mid"), nb::arg("call_high"), nb::arg("strike_spacing"),
        nb::arg("rate"), nb::arg("t"));
  m.def("lognormal_density", &pricers::LognormalDensity, nb::arg("spot"),
        nb::arg("strike"), nb::arg("rate"), nb::arg("yield_"),
        nb::arg("volatility"), nb::arg("t"));
  m.def("binary_jump_price", &pricers::BinaryJumpPrice, nb::arg("option_type"),
        nb::arg("strike"), nb::arg("rate"), nb::arg("t"), nb::arg("up"),
        nb::arg("down"), nb::arg("p_up"));

  // Chapter 16: employee stock options.
  using pricers::EsoTerms;
  nb::class_<EsoTerms>(m, "EsoTerms")
      .def(
          "__init__",
          [](EsoTerms* self, double strike, double maturity, double vesting,
             double exercise_multiple, double exit_rate) {
            new (self) EsoTerms{strike, maturity, vesting, exercise_multiple,
                                exit_rate};
          },
          nb::arg("strike") = 0.0, nb::arg("maturity") = 0.0,
          nb::arg("vesting") = 0.0, nb::arg("exercise_multiple") = 0.0,
          nb::arg("exit_rate") = 0.0)
      .def_rw("strike", &EsoTerms::strike)
      .def_rw("maturity", &EsoTerms::maturity)
      .def_rw("vesting", &EsoTerms::vesting)
      .def_rw("exercise_multiple", &EsoTerms::exercise_multiple)
      .def_rw("exit_rate", &EsoTerms::exit_rate);
  m.def(
      "price_employee_stock_option",
      [](const EsoTerms& terms, const TreeMarket& market, int steps) {
        const auto v = pricers::PriceEmployeeStockOption(terms, market, steps);
        if (!v) throw nb::value_error("invalid employee stock option terms");
        return *v;
      },
      nb::arg("terms"), nb::arg("market"), nb::arg("steps"));
  m.def("expected_life_value", &pricers::ExpectedLifeValue, nb::arg("spot"),
        nb::arg("strike"), nb::arg("rate"), nb::arg("yield_"),
        nb::arg("volatility"), nb::arg("expected_life"));
}

}  // namespace ofod::bindings
