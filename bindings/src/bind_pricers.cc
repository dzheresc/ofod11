// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <vector>

#include "bindings.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/bond_pricer.h"
#include "ofod/pricers/bounds.h"
#include "ofod/pricers/forward_pricer.h"
#include "ofod/pricers/fra_pricer.h"
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
}

}  // namespace ofod::bindings
