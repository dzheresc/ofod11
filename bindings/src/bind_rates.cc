// Copyright 2026 Denis Zhereshchin
//
// Chapters 29 to 34: interest rate options, adjustments, short-rate models
// and trees, the LIBOR market model, HJM, MBS and nonstandard swaps.

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "bindings.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/models/lmm.h"
#include "ofod/models/short_rate.h"
#include "ofod/pricers/adjustments.h"
#include "ofod/pricers/exotic_swaps.h"
#include "ofod/pricers/mbs.h"
#include "ofod/pricers/rate_options.h"
#include "ofod/pricers/short_rate_tree.h"

namespace ofod::bindings {

namespace nb = nanobind;

namespace {

using curves::ZeroCurve;
using instruments::Exercise;
using instruments::OptionType;

template <typename T>
T Unwrap(std::optional<T> v, const char* message) {
  if (!v) throw nb::value_error(message);
  return *std::move(v);
}

void BindRateOptions(nb::module_& m) {
  namespace p = pricers;
  m.def("forward_bond_price", &p::ForwardBondPrice, nb::arg("spot_price"),
        nb::arg("coupon_pv"), nb::arg("discount"));
  m.def("black_formula", &p::BlackFormula, nb::arg("option_type"),
        nb::arg("forward"), nb::arg("strike"), nb::arg("volatility"),
        nb::arg("t"));
  m.def("bachelier_formula", &p::BachelierFormula, nb::arg("option_type"),
        nb::arg("forward"), nb::arg("strike"), nb::arg("normal_vol"),
        nb::arg("t"));
  m.def("black_bond_option_price", &p::BlackBondOptionPrice,
        nb::arg("option_type"), nb::arg("forward_price"), nb::arg("strike"),
        nb::arg("discount"), nb::arg("volatility"), nb::arg("t"));
  m.def("price_volatility_from_yield", &p::PriceVolatilityFromYield,
        nb::arg("modified_duration"), nb::arg("forward_yield"),
        nb::arg("yield_volatility"));
  m.def("black_caplet_price", &p::BlackCapletPrice, nb::arg("option_type"),
        nb::arg("forward"), nb::arg("strike"), nb::arg("volatility"),
        nb::arg("reset"), nb::arg("accrual"), nb::arg("discount"),
        nb::arg("notional"));

  using p::CapTerms;
  nb::class_<CapTerms>(m, "CapTerms")
      .def(
          "__init__",
          [](CapTerms* self, double strike, double maturity, int frequency,
             double notional) {
            new (self) CapTerms{strike, maturity, frequency, notional};
          },
          nb::arg("strike"), nb::arg("maturity"), nb::arg("frequency") = 4,
          nb::arg("notional") = 1.0)
      .def_rw("strike", &CapTerms::strike)
      .def_rw("maturity", &CapTerms::maturity)
      .def_rw("frequency", &CapTerms::frequency)
      .def_rw("notional", &CapTerms::notional);
  m.def("caplet_count", &p::CapletCount, nb::arg("terms"));
  m.def(
      "caplet_prices",
      [](OptionType type, const ZeroCurve& curve, const CapTerms& terms,
         const std::vector<double>& vols) {
        return Unwrap(p::CapletPrices(type, curve, terms, vols),
                      "bad cap schedule or volatilities");
      },
      nb::arg("option_type"), nb::arg("curve"), nb::arg("terms"),
      nb::arg("volatilities"));
  m.def(
      "cap_floor_price",
      [](OptionType type, const ZeroCurve& curve, const CapTerms& terms,
         double vol) {
        return Unwrap(p::CapFloorPrice(type, curve, terms, vol),
                      "bad cap schedule");
      },
      nb::arg("option_type"), nb::arg("curve"), nb::arg("terms"),
      nb::arg("flat_volatility"));
  m.def(
      "strip_caplet_volatilities",
      [](const ZeroCurve& curve, double strike, int frequency,
         const std::vector<double>& flat_vols, double notional) {
        return Unwrap(p::StripCapletVolatilities(curve, strike, frequency,
                                                 flat_vols, notional),
                      "caplet volatility not found");
      },
      nb::arg("curve"), nb::arg("strike"), nb::arg("frequency"),
      nb::arg("flat_vols"), nb::arg("notional") = 1.0);

  nb::class_<p::ForwardSwap>(m, "ForwardSwap")
      .def(nb::init<double, double>(), nb::arg("annuity"), nb::arg("rate"))
      .def_rw("annuity", &p::ForwardSwap::annuity)
      .def_rw("rate", &p::ForwardSwap::rate);
  m.def("forward_swap_rate", &p::ForwardSwapRate, nb::arg("curve"),
        nb::arg("start"), nb::arg("tenor"), nb::arg("frequency"));
  m.def("black_swaption_price", &p::BlackSwaptionPrice, nb::arg("option_type"),
        nb::arg("forward_rate"), nb::arg("strike"), nb::arg("volatility"),
        nb::arg("expiry"), nb::arg("annuity"), nb::arg("notional"));
  m.def("bachelier_swaption_price", &p::BachelierSwaptionPrice,
        nb::arg("option_type"), nb::arg("forward_rate"), nb::arg("strike"),
        nb::arg("normal_vol"), nb::arg("expiry"), nb::arg("annuity"),
        nb::arg("notional"));
  m.def("bucket_dv01", &p::BucketDv01, nb::arg("value"), nb::arg("curve"),
        nb::arg("bump") = 1e-4);
  m.def("parallel_dv01", &p::ParallelDv01, nb::arg("value"), nb::arg("curve"),
        nb::arg("bump") = 1e-4);
}

void BindAdjustments(nb::module_& m) {
  namespace p = pricers;
  m.def("convexity_adjustment", &p::ConvexityAdjustment,
        nb::arg("forward_yield"), nb::arg("yield_volatility"), nb::arg("t"),
        nb::arg("g_first"), nb::arg("g_second"));
  m.def("bond_yield_convexity_adjustment", &p::BondYieldConvexityAdjustment,
        nb::arg("forward_yield"), nb::arg("yield_volatility"), nb::arg("t"),
        nb::arg("coupon_rate"), nb::arg("maturity"), nb::arg("frequency"));
  m.def("timing_adjustment_factor", &p::TimingAdjustmentFactor,
        nb::arg("forward_rate"), nb::arg("forward_volatility"),
        nb::arg("variable_volatility"), nb::arg("correlation"), nb::arg("t"),
        nb::arg("tau"));
  m.def("quanto_adjustment_factor", &p::QuantoAdjustmentFactor,
        nb::arg("correlation"), nb::arg("exchange_volatility"),
        nb::arg("variable_volatility"), nb::arg("t"));
  m.def("in_arrears_rate", &p::InArrearsRate, nb::arg("forward_rate"),
        nb::arg("volatility"), nb::arg("t"), nb::arg("tau"));
  using p::CmsTerms;
  nb::class_<CmsTerms>(m, "CmsTerms")
      .def(
          "__init__",
          [](CmsTerms* self, double forward_swap_rate, double swap_volatility,
             double t, double swap_tenor, int frequency, double delay,
             double delay_rate, double delay_volatility, double correlation) {
            new (self)
                CmsTerms{forward_swap_rate, swap_volatility,  t,
                         swap_tenor,        frequency,        delay,
                         delay_rate,        delay_volatility, correlation};
          },
          nb::arg("forward_swap_rate"), nb::arg("swap_volatility"),
          nb::arg("t"), nb::arg("swap_tenor"), nb::arg("frequency"),
          nb::arg("delay") = 0.0, nb::arg("delay_rate") = 0.0,
          nb::arg("delay_volatility") = 0.0, nb::arg("correlation") = 1.0)
      .def_rw("forward_swap_rate", &CmsTerms::forward_swap_rate)
      .def_rw("swap_volatility", &CmsTerms::swap_volatility)
      .def_rw("t", &CmsTerms::t)
      .def_rw("swap_tenor", &CmsTerms::swap_tenor)
      .def_rw("frequency", &CmsTerms::frequency)
      .def_rw("delay", &CmsTerms::delay)
      .def_rw("delay_rate", &CmsTerms::delay_rate)
      .def_rw("delay_volatility", &CmsTerms::delay_volatility)
      .def_rw("correlation", &CmsTerms::correlation);
  m.def("cms_rate", &p::CmsRate, nb::arg("terms"));
}

void BindShortRateTree(nb::module_& m) {
  namespace p = pricers;
  nb::enum_<p::ShortRateModel>(m, "ShortRateModel")
      .value("HULL_WHITE", p::ShortRateModel::kHullWhite)
      .value("BLACK_KARASINSKI", p::ShortRateModel::kBlackKarasinski);
  nb::class_<p::Branch>(m, "Branch")
      .def(nb::init<int, double, double, double>(), nb::arg("middle"),
           nb::arg("up"), nb::arg("mid"), nb::arg("down"))
      .def_rw("middle", &p::Branch::middle)
      .def_rw("up", &p::Branch::up)
      .def_rw("mid", &p::Branch::mid)
      .def_rw("down", &p::Branch::down);
  nb::class_<p::TreeCashFlow>(m, "TreeCashFlow")
      .def(nb::init<int, double>(), nb::arg("step"), nb::arg("amount"))
      .def_rw("step", &p::TreeCashFlow::step)
      .def_rw("amount", &p::TreeCashFlow::amount);
  using p::ShortRateTree;
  nb::class_<ShortRateTree>(m, "ShortRateTree")
      .def(
          "__init__",
          [](ShortRateTree* self, const ZeroCurve& curve, double a,
             double sigma, double dt, int steps, p::ShortRateModel model) {
            new (self) ShortRateTree(
                Unwrap(ShortRateTree::Build(curve, a, sigma, dt, steps, model),
                       "invalid tree inputs"));
          },
          nb::arg("curve"), nb::arg("a"), nb::arg("sigma"), nb::arg("dt"),
          nb::arg("steps"), nb::arg("model") = p::ShortRateModel::kHullWhite)
      .def_prop_ro("steps", &ShortRateTree::steps)
      .def_prop_ro("dt", &ShortRateTree::dt)
      .def_prop_ro("dx", &ShortRateTree::dx)
      .def_prop_ro("jmax", &ShortRateTree::jmax)
      .def_prop_ro("alphas",
                   [](const ShortRateTree& t) {
                     return std::vector<double>(t.alphas().begin(),
                                                t.alphas().end());
                   })
      .def("width", &ShortRateTree::Width, nb::arg("step"))
      .def("rate", &ShortRateTree::Rate, nb::arg("step"), nb::arg("j"))
      .def("discount", &ShortRateTree::Discount, nb::arg("step"), nb::arg("j"))
      .def("branch_at", &ShortRateTree::BranchAt, nb::arg("j"))
      .def(
          "arrow_debreu",
          [](const ShortRateTree& t, int step) {
            const auto q = t.ArrowDebreu(step);
            return std::vector<double>(q.begin(), q.end());
          },
          nb::arg("step"))
      .def(
          "rollback",
          [](const ShortRateTree& t, const std::vector<double>& next,
             int step) { return t.Rollback(next, step); },
          nb::arg("nxt"), nb::arg("step"))
      .def("zero_bond_values", &ShortRateTree::ZeroBondValues, nb::arg("step"),
           nb::arg("maturity_step"));
  m.def(
      "tree_bond_option",
      [](const ShortRateTree& tree, OptionType type, Exercise exercise,
         int expiry_step, const std::vector<p::TreeCashFlow>& bond,
         double strike) {
        return Unwrap(
            p::TreeBondOption(tree, type, exercise, expiry_step, bond, strike),
            "bad cash flows or expiry");
      },
      nb::arg("tree"), nb::arg("option_type"), nb::arg("exercise"),
      nb::arg("expiry_step"), nb::arg("bond"), nb::arg("strike"));
  m.def(
      "tree_swaption",
      [](const ShortRateTree& tree, OptionType type, double strike,
         int period_steps, int end_step,
         const std::vector<int>& exercise_steps) {
        return Unwrap(p::TreeSwaption(tree, type, strike, period_steps,
                                      end_step, exercise_steps),
                      "bad schedule");
      },
      nb::arg("tree"), nb::arg("option_type"), nb::arg("strike"),
      nb::arg("period_steps"), nb::arg("end_step"), nb::arg("exercise_steps"));
}

void BindMbs(nb::module_& m) {
  namespace p = pricers;
  using p::MortgagePool;
  nb::class_<MortgagePool>(m, "MortgagePool")
      .def(
          "__init__",
          [](MortgagePool* self, double balance, double mortgage_rate,
             double pass_through_rate, int months, int age) {
            new (self) MortgagePool{balance, mortgage_rate, pass_through_rate,
                                    months, age};
          },
          nb::arg("balance"), nb::arg("mortgage_rate"),
          nb::arg("pass_through_rate"), nb::arg("months"), nb::arg("age") = 0)
      .def_rw("balance", &MortgagePool::balance)
      .def_rw("mortgage_rate", &MortgagePool::mortgage_rate)
      .def_rw("pass_through_rate", &MortgagePool::pass_through_rate)
      .def_rw("months", &MortgagePool::months)
      .def_rw("age", &MortgagePool::age);
  using p::MbsCashFlow;
  nb::class_<MbsCashFlow>(m, "MbsCashFlow")
      .def(nb::init<int, double, double, double, double>(), nb::arg("month"),
           nb::arg("interest"), nb::arg("scheduled"), nb::arg("prepaid"),
           nb::arg("balance"))
      .def_rw("month", &MbsCashFlow::month)
      .def_rw("interest", &MbsCashFlow::interest)
      .def_rw("scheduled", &MbsCashFlow::scheduled)
      .def_rw("prepaid", &MbsCashFlow::prepaid)
      .def_rw("balance", &MbsCashFlow::balance);
  using p::MbsValue;
  nb::class_<MbsValue>(m, "MbsValue")
      .def(nb::init<double, double, double>(), nb::arg("pass_through"),
           nb::arg("interest_only"), nb::arg("principal_only"))
      .def_rw("pass_through", &MbsValue::pass_through)
      .def_rw("interest_only", &MbsValue::interest_only)
      .def_rw("principal_only", &MbsValue::principal_only);
  using p::RefinancingModel;
  nb::class_<RefinancingModel>(m, "RefinancingModel")
      .def(
          "__init__",
          [](RefinancingModel* self, double base_cpr, double sensitivity,
             double spread, double max_cpr) {
            new (self) RefinancingModel{base_cpr, sensitivity, spread, max_cpr};
          },
          nb::arg("base_cpr") = 0.06, nb::arg("sensitivity") = 5.0,
          nb::arg("spread") = 0.015, nb::arg("max_cpr") = 0.6)
      .def_rw("base_cpr", &RefinancingModel::base_cpr)
      .def_rw("sensitivity", &RefinancingModel::sensitivity)
      .def_rw("spread", &RefinancingModel::spread)
      .def_rw("max_cpr", &RefinancingModel::max_cpr);
  using p::MbsMcOptions;
  nb::class_<MbsMcOptions>(m, "MbsMcOptions")
      .def(
          "__init__",
          [](MbsMcOptions* self, int paths, std::uint64_t seed) {
            new (self) MbsMcOptions{paths, seed};
          },
          nb::arg("paths") = 5'000, nb::arg("seed") = 1)
      .def_rw("paths", &MbsMcOptions::paths)
      .def_rw("seed", &MbsMcOptions::seed);
  m.def("psa_cpr", &p::PsaCpr, nb::arg("month"), nb::arg("speed"));
  m.def("monthly_mortality", &p::MonthlyMortality, nb::arg("cpr"));
  m.def("pass_through_cash_flows", &p::PassThroughCashFlows, nb::arg("pool"),
        nb::arg("psa_speed"));
  m.def("value_cash_flows", &p::ValueCashFlows, nb::arg("flows"),
        nb::arg("curve"));
  m.def(
      "mbs_monte_carlo",
      [](const MortgagePool& pool, const models::HullWhite& rates,
         std::optional<RefinancingModel> model,
         std::optional<MbsMcOptions> options) {
        return Unwrap(
            p::MbsMonteCarlo(pool, rates, model.value_or(RefinancingModel{}),
                             options.value_or(MbsMcOptions{})),
            "need paths and months");
      },
      nb::arg("pool"), nb::arg("rates"), nb::arg("model") = nb::none(),
      nb::arg("options") = nb::none());
}

void BindExoticSwaps(nb::module_& m) {
  namespace p = pricers;
  using p::SwapPeriod;
  nb::class_<SwapPeriod>(m, "SwapPeriod")
      .def(nb::init<double, double, double, double>(), nb::arg("start"),
           nb::arg("end"), nb::arg("notional"), nb::arg("fixed_rate"))
      .def_rw("start", &SwapPeriod::start)
      .def_rw("end", &SwapPeriod::end)
      .def_rw("notional", &SwapPeriod::notional)
      .def_rw("fixed_rate", &SwapPeriod::fixed_rate);
  using p::CmsLeg;
  nb::class_<CmsLeg>(m, "CmsLeg")
      .def(
          "__init__",
          [](CmsLeg* self, double tenor, int frequency, double forward_vol,
             double correlation) {
            new (self) CmsLeg{tenor, frequency, forward_vol, correlation};
          },
          nb::arg("cms_tenor"), nb::arg("cms_frequency"),
          nb::arg("forward_vol"), nb::arg("correlation") = 1.0)
      .def_rw("cms_tenor", &CmsLeg::cms_tenor)
      .def_rw("cms_frequency", &CmsLeg::cms_frequency)
      .def_rw("forward_vol", &CmsLeg::forward_vol)
      .def_rw("correlation", &CmsLeg::correlation);
  m.def("swap_schedule", &p::SwapSchedule, nb::arg("start"),
        nb::arg("maturity"), nb::arg("frequency"), nb::arg("notional"),
        nb::arg("fixed_rate"), nb::arg("amortization") = 0.0,
        nb::arg("step_up") = 0.0);
  m.def(
      "schedule_swap_value",
      [](const ZeroCurve& curve, const std::vector<SwapPeriod>& periods) {
        return p::ScheduleSwapValue(curve, periods);
      },
      nb::arg("curve"), nb::arg("periods"));
  m.def(
      "par_fixed_rate",
      [](const ZeroCurve& curve, const std::vector<SwapPeriod>& periods) {
        return p::ParFixedRate(curve, periods);
      },
      nb::arg("curve"), nb::arg("periods"));
  m.def(
      "in_arrears_swap_value",
      [](const ZeroCurve& curve, const std::vector<SwapPeriod>& periods,
         const std::vector<double>& vols) {
        if (vols.size() < periods.size()) {
          throw nb::value_error("need one vol per period");
        }
        return p::InArrearsSwapValue(curve, periods, vols);
      },
      nb::arg("curve"), nb::arg("periods"), nb::arg("vols"));
  m.def(
      "cms_swap_value",
      [](const ZeroCurve& curve, const std::vector<SwapPeriod>& periods,
         const CmsLeg& leg, const std::vector<double>& vols) {
        if (vols.size() < periods.size()) {
          throw nb::value_error("need one vol per period");
        }
        return p::CmsSwapValue(curve, periods, leg, vols);
      },
      nb::arg("curve"), nb::arg("periods"), nb::arg("leg"),
      nb::arg("swap_vols"));
  m.def(
      "accrual_fixed_leg_value",
      [](const ZeroCurve& curve, const std::vector<SwapPeriod>& periods,
         double low, double high, double vol, int observations) {
        return p::AccrualFixedLegValue(curve, periods, low, high, vol,
                                       observations);
      },
      nb::arg("curve"), nb::arg("periods"), nb::arg("low"), nb::arg("high"),
      nb::arg("vol"), nb::arg("observations"));
}

void BindShortRateModels(nb::module_& m) {
  namespace md = models;
  using md::Cir;
  using md::Vasicek;
  nb::class_<Vasicek>(m, "Vasicek")
      .def(nb::init<double, double, double>(), nb::arg("a"), nb::arg("b"),
           nb::arg("sigma"))
      .def_rw("a", &Vasicek::a)
      .def_rw("b", &Vasicek::b)
      .def_rw("sigma", &Vasicek::sigma);
  nb::class_<Cir>(m, "Cir")
      .def(nb::init<double, double, double>(), nb::arg("a"), nb::arg("b"),
           nb::arg("sigma"))
      .def_rw("a", &Cir::a)
      .def_rw("b", &Cir::b)
      .def_rw("sigma", &Cir::sigma);
  m.def("bond_price",
        nb::overload_cast<const Vasicek&, double, double>(&md::BondPrice),
        nb::arg("model"), nb::arg("r"), nb::arg("tau"));
  m.def("bond_price",
        nb::overload_cast<const Cir&, double, double>(&md::BondPrice),
        nb::arg("model"), nb::arg("r"), nb::arg("tau"));
  m.def("zero_rate",
        nb::overload_cast<const Vasicek&, double, double>(&md::ZeroRate),
        nb::arg("model"), nb::arg("r"), nb::arg("tau"));
  m.def("zero_rate",
        nb::overload_cast<const Cir&, double, double>(&md::ZeroRate),
        nb::arg("model"), nb::arg("r"), nb::arg("tau"));
  m.def("zero_bond_option_formula", &md::ZeroBondOptionFormula,
        nb::arg("option_type"), nb::arg("p_expiry"), nb::arg("p_maturity"),
        nb::arg("strike"), nb::arg("sigma_p"));
  m.def("zero_bond_option_price", &md::ZeroBondOptionPrice, nb::arg("model"),
        nb::arg("option_type"), nb::arg("r"), nb::arg("expiry"),
        nb::arg("maturity"), nb::arg("strike"));
  m.def(
      "estimate_vasicek",
      [](const std::vector<double>& rates, double dt) {
        return Unwrap(md::EstimateVasicek(rates, dt),
                      "need at least 3 rates with mean reversion");
      },
      nb::arg("rates"), nb::arg("dt"));
  m.def("simulate_cir", &md::SimulateCir, nb::arg("model"), nb::arg("r0"),
        nb::arg("dt"), nb::arg("steps"), nb::arg("seed"));

  using md::HullWhite;
  nb::class_<HullWhite>(m, "HullWhite")
      .def(
          "__init__",
          [](HullWhite* self, const ZeroCurve& curve, double a, double sigma) {
            new (self) HullWhite(Unwrap(HullWhite::Create(curve, a, sigma),
                                        "need a >= 0 and sigma > 0"));
          },
          nb::arg("curve"), nb::arg("a"), nb::arg("sigma"))
      .def_prop_ro("a", &HullWhite::a)
      .def_prop_ro("sigma", &HullWhite::sigma)
      .def_prop_ro("curve", &HullWhite::curve)
      .def("b", &HullWhite::B, nb::arg("t"), nb::arg("maturity"))
      .def("instantaneous_forward", &HullWhite::InstantaneousForward,
           nb::arg("t"))
      .def("bond_price", &HullWhite::BondPrice, nb::arg("t"),
           nb::arg("maturity"), nb::arg("r"))
      .def("bond_option_volatility", &HullWhite::BondOptionVolatility,
           nb::arg("expiry"), nb::arg("maturity"))
      .def("zero_bond_option", &HullWhite::ZeroBondOption,
           nb::arg("option_type"), nb::arg("expiry"), nb::arg("maturity"),
           nb::arg("strike"))
      .def(
          "coupon_bond_option",
          [](const HullWhite& hw, OptionType type, double expiry,
             const std::vector<double>& times,
             const std::vector<double>& amounts, double strike) {
            return Unwrap(
                hw.CouponBondOption(type, expiry, times, amounts, strike),
                "bad cash flows or no critical rate");
          },
          nb::arg("option_type"), nb::arg("expiry"), nb::arg("times"),
          nb::arg("amounts"), nb::arg("strike"))
      .def("caplet", &HullWhite::Caplet, nb::arg("option_type"),
           nb::arg("reset"), nb::arg("accrual"), nb::arg("strike"),
           nb::arg("notional"));

  using md::CapletQuote;
  nb::class_<CapletQuote>(m, "CapletQuote")
      .def(nb::init<double, double, double, double>(), nb::arg("reset"),
           nb::arg("accrual"), nb::arg("strike"), nb::arg("market_price"))
      .def_rw("reset", &CapletQuote::reset)
      .def_rw("accrual", &CapletQuote::accrual)
      .def_rw("strike", &CapletQuote::strike)
      .def_rw("market_price", &CapletQuote::market_price);
  using md::HullWhiteFit;
  nb::class_<HullWhiteFit>(m, "HullWhiteFit")
      .def(nb::init<double, double, double>(), nb::arg("a"), nb::arg("sigma"),
           nb::arg("rms_relative_error"))
      .def_rw("a", &HullWhiteFit::a)
      .def_rw("sigma", &HullWhiteFit::sigma)
      .def_rw("rms_relative_error", &HullWhiteFit::rms_relative_error);
  m.def(
      "calibrate_hull_white",
      [](const ZeroCurve& curve, const std::vector<CapletQuote>& quotes,
         double a0, double sigma0) {
        return Unwrap(md::CalibrateHullWhite(curve, quotes, a0, sigma0),
                      "need quotes and positive starting values");
      },
      nb::arg("curve"), nb::arg("quotes"), nb::arg("a0") = 0.1,
      nb::arg("sigma0") = 0.01);
}

void BindForwardRateModels(nb::module_& m) {
  namespace md = models;
  using md::LmmSpec;
  nb::class_<LmmSpec>(m, "LmmSpec")
      .def(nb::init<double, std::vector<double>, md::Matrix>(),
           nb::arg("accrual"), nb::arg("forwards"), nb::arg("loadings"))
      .def_rw("accrual", &LmmSpec::accrual)
      .def_rw("forwards", &LmmSpec::forwards)
      .def_rw("loadings", &LmmSpec::loadings);
  m.def(
      "lmm_volatilities_from_caplets",
      [](const std::vector<double>& vols) {
        return Unwrap(md::LmmVolatilitiesFromCaplets(vols),
                      "caplet vols imply negative variance");
      },
      nb::arg("caplet_vols"));
  m.def(
      "one_factor_loadings",
      [](const std::vector<double>& l) { return md::OneFactorLoadings(l); },
      nb::arg("lambdas"));
  m.def("lmm_discount_factors", &md::LmmDiscountFactors, nb::arg("spec"));
  using md::LmmPath;
  // One path; the pure-Python class holds all paths as arrays, so payoffs
  // written with NumPy ufuncs work with both.
  nb::class_<LmmPath>(m, "LmmPaths")
      .def_prop_ro("size", &LmmPath::size)
      .def("forward", &LmmPath::Forward, nb::arg("j"), nb::arg("k"))
      .def("reset", &LmmPath::Reset, nb::arg("k"))
      .def("numeraire", &LmmPath::Numeraire, nb::arg("j"));
  using md::LmmOptions;
  nb::class_<LmmOptions>(m, "LmmOptions")
      .def(
          "__init__",
          [](LmmOptions* self, int paths, std::uint64_t seed, bool pc) {
            new (self) LmmOptions{paths, seed, pc};
          },
          nb::arg("paths") = 10'000, nb::arg("seed") = 1,
          nb::arg("predictor_corrector") = false)
      .def_rw("paths", &LmmOptions::paths)
      .def_rw("seed", &LmmOptions::seed)
      .def_rw("predictor_corrector", &LmmOptions::predictor_corrector);
  m.def(
      "lmm_monte_carlo",
      [](const LmmSpec& spec, const std::function<double(const LmmPath&)>& f,
         std::optional<LmmOptions> options) {
        return Unwrap(
            md::LmmMonteCarlo(spec, f, options.value_or(LmmOptions{})),
            "inconsistent LMM spec");
      },
      nb::arg("spec"), nb::arg("payoff"), nb::arg("options") = nb::none());
  m.def(
      "rebonato_swaption_volatility",
      [](const LmmSpec& spec, int start, int end) {
        return Unwrap(md::RebonatoSwaptionVolatility(spec, start, end),
                      "bad swap indices");
      },
      nb::arg("spec"), nb::arg("start"), nb::arg("end"));
  using md::HjmSpec;
  nb::class_<HjmSpec>(m, "HjmSpec")
      .def(nb::init<double, std::vector<double>,
                    std::function<double(double, double)>>(),
           nb::arg("dt"), nb::arg("forwards"), nb::arg("volatility"))
      .def_rw("dt", &HjmSpec::dt)
      .def_rw("forwards", &HjmSpec::forwards);
  m.def(
      "hjm_zero_bond_option",
      [](const HjmSpec& spec, OptionType type, int expiry, int maturity,
         double strike, int paths, std::uint64_t seed) {
        return Unwrap(md::HjmZeroBondOption(spec, type, expiry, maturity,
                                            strike, paths, seed),
                      "bad HJM inputs");
      },
      nb::arg("spec"), nb::arg("option_type"), nb::arg("expiry"),
      nb::arg("maturity"), nb::arg("strike"), nb::arg("paths"),
      nb::arg("seed"));
}

}  // namespace

void BindRates(nb::module_& pricers, nb::module_& models) {
  BindShortRateModels(models);
  BindForwardRateModels(models);
  BindRateOptions(pricers);
  BindAdjustments(pricers);
  BindShortRateTree(pricers);
  BindMbs(pricers);
  BindExoticSwaps(pricers);
}

}  // namespace ofod::bindings
