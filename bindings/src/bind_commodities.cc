// Copyright 2026 Denis Zhereshchin
//
// Chapters 35 and 36: commodity curves, trees and spikes, weather and
// insurance derivatives, and real options.

#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "bindings.h"
#include "ofod/models/commodity.h"
#include "ofod/pricers/commodity_tree.h"
#include "ofod/pricers/real_options.h"
#include "ofod/pricers/weather_insurance.h"

namespace ofod::bindings {

namespace nb = nanobind;

namespace {

using instruments::Exercise;
using instruments::OptionType;

template <typename T>
T Unwrap(std::optional<T> v, const char* message) {
  if (!v) throw nb::value_error(message);
  return *std::move(v);
}

void BindCommodityModels(nb::module_& m) {
  namespace md = models;
  using md::SeasonalCurve;
  nb::class_<SeasonalCurve>(m, "SeasonalCurve")
      .def(
          "__init__",
          [](SeasonalCurve* self, double level, double growth,
             std::optional<std::array<double, 12>> factors) {
            new (self) SeasonalCurve{
                level, growth, factors.value_or(std::array<double, 12>{})};
          },
          nb::arg("level") = 0.0, nb::arg("growth") = 0.0,
          nb::arg("factors") = nb::none())
      .def_rw("level", &SeasonalCurve::level)
      .def_rw("growth", &SeasonalCurve::growth)
      .def_rw("factors", &SeasonalCurve::factors)
      .def("forward", &SeasonalCurve::Forward, nb::arg("t"))
      .def("multiplier", &SeasonalCurve::Multiplier, nb::arg("t"));
  m.def(
      "fit_seasonal_curve",
      [](const std::vector<double>& maturities,
         const std::vector<double>& futures) {
        return Unwrap(md::FitSeasonalCurve(maturities, futures),
                      "need 14 positive futures covering every month");
      },
      nb::arg("maturities"), nb::arg("futures"));
  using md::SpikeModel;
  nb::class_<SpikeModel>(m, "SpikeModel")
      .def(nb::init<double, double, double, double, double, double>(),
           nb::arg("log_level") = 0.0, nb::arg("a") = 0.0,
           nb::arg("sigma") = 0.0, nb::arg("intensity") = 0.0,
           nb::arg("jump_mean") = 0.0, nb::arg("jump_vol") = 0.0)
      .def_rw("log_level", &SpikeModel::log_level)
      .def_rw("a", &SpikeModel::a)
      .def_rw("sigma", &SpikeModel::sigma)
      .def_rw("intensity", &SpikeModel::intensity)
      .def_rw("jump_mean", &SpikeModel::jump_mean)
      .def_rw("jump_vol", &SpikeModel::jump_vol);
  using md::SpikeEstimate;
  nb::class_<SpikeEstimate>(m, "SpikeEstimate")
      .def(nb::init<double, double>(), nb::arg("mean"), nb::arg("std_error"))
      .def_rw("mean", &SpikeEstimate::mean)
      .def_rw("std_error", &SpikeEstimate::std_error);
  m.def(
      "spike_expected_price",
      [](const SpikeModel& model, double x0, double t) {
        return Unwrap(md::SpikeExpectedPrice(model, x0, t), "invalid model");
      },
      nb::arg("model"), nb::arg("x0"), nb::arg("t"));
  m.def(
      "simulate_spike_path",
      [](const SpikeModel& model, double x0, double dt, int steps,
         std::uint64_t seed) {
        return Unwrap(md::SimulateSpikePath(model, x0, dt, steps, seed),
                      "invalid inputs");
      },
      nb::arg("model"), nb::arg("x0"), nb::arg("dt"), nb::arg("steps"),
      nb::arg("seed"));
  m.def(
      "spike_monte_carlo",
      [](const SpikeModel& model, double x0, double t, int steps, int paths,
         std::uint64_t seed) {
        return Unwrap(md::SpikeMonteCarlo(model, x0, t, steps, paths, seed),
                      "invalid inputs");
      },
      nb::arg("model"), nb::arg("x0"), nb::arg("t"), nb::arg("steps"),
      nb::arg("paths"), nb::arg("seed"));
}

void BindCommodityTree(nb::module_& m) {
  namespace p = pricers;
  using p::CommodityTree;
  nb::class_<CommodityTree>(m, "CommodityTree")
      .def(
          "__init__",
          [](CommodityTree* self, double spot,
             const std::vector<double>& futures, double a, double sigma,
             double dt) {
            new (self) CommodityTree(
                Unwrap(CommodityTree::Build(spot, futures, a, sigma, dt),
                       "invalid commodity tree inputs"));
          },
          nb::arg("spot"), nb::arg("futures"), nb::arg("a"), nb::arg("sigma"),
          nb::arg("dt"))
      .def_prop_ro("steps", &CommodityTree::steps)
      .def_prop_ro("dt", &CommodityTree::dt)
      .def_prop_ro("dx", &CommodityTree::dx)
      .def_prop_ro("jmax", &CommodityTree::jmax)
      .def_prop_ro("alphas",
                   [](const CommodityTree& t) {
                     return std::vector<double>(t.alphas().begin(),
                                                t.alphas().end());
                   })
      .def("width", &CommodityTree::Width, nb::arg("step"))
      .def("price", &CommodityTree::Price, nb::arg("step"), nb::arg("j"))
      .def("branch_at", &CommodityTree::BranchAt, nb::arg("j"))
      .def(
          "probabilities",
          [](const CommodityTree& t, int step) {
            const auto p = t.Probabilities(step);
            return std::vector<double>(p.begin(), p.end());
          },
          nb::arg("step"))
      .def(
          "rollback",
          [](const CommodityTree& t, const std::vector<double>& next, int step,
             double rate) { return t.Rollback(next, step, rate); },
          nb::arg("nxt"), nb::arg("step"), nb::arg("rate"));
  m.def(
      "commodity_tree_option",
      [](const CommodityTree& tree, OptionType type, Exercise exercise,
         double strike, int expiry_step, double rate) {
        return Unwrap(p::CommodityTreeOption(tree, type, exercise, strike,
                                             expiry_step, rate),
                      "expiry step outside the tree");
      },
      nb::arg("tree"), nb::arg("option_type"), nb::arg("exercise"),
      nb::arg("strike"), nb::arg("expiry_step"), nb::arg("rate"));
}

void BindWeather(nb::module_& m) {
  namespace p = pricers;
  nb::enum_<p::DegreeDayKind>(m, "DegreeDayKind")
      .value("HEATING", p::DegreeDayKind::kHeating)
      .value("COOLING", p::DegreeDayKind::kCooling);
  m.def(
      "degree_days",
      [](p::DegreeDayKind kind, const std::vector<double>& temperatures,
         double base) { return p::DegreeDays(kind, temperatures, base); },
      nb::arg("kind"), nb::arg("temperatures"), nb::arg("base") = 65.0);
  using p::DegreeDayOption;
  nb::class_<DegreeDayOption>(m, "DegreeDayOption")
      .def(nb::init<OptionType, double, double, double>(),
           nb::arg("option_type") = OptionType::kCall, nb::arg("strike") = 0.0,
           nb::arg("tick") = 1.0,
           nb::arg("cap") = std::numeric_limits<double>::infinity())
      .def_rw("option_type", &DegreeDayOption::type)
      .def_rw("strike", &DegreeDayOption::strike)
      .def_rw("tick", &DegreeDayOption::tick)
      .def_rw("cap", &DegreeDayOption::cap);
  m.def("degree_day_payoff", &p::DegreeDayPayoff, nb::arg("option"),
        nb::arg("degree_days"));
  using p::ExpectationEstimate;
  nb::class_<ExpectationEstimate>(m, "ExpectationEstimate")
      .def(nb::init<double, double>(), nb::arg("value"), nb::arg("std_error"))
      .def_rw("value", &ExpectationEstimate::value)
      .def_rw("std_error", &ExpectationEstimate::std_error);
  m.def(
      "burn_analysis",
      [](const DegreeDayOption& option, const std::vector<double>& seasons,
         double rate, double maturity) {
        return Unwrap(p::BurnAnalysis(option, seasons, rate, maturity),
                      "need at least two seasons");
      },
      nb::arg("option"), nb::arg("season_degree_days"), nb::arg("rate"),
      nb::arg("maturity"));
  using p::TemperatureModel;
  nb::class_<TemperatureModel>(m, "TemperatureModel")
      .def(nb::init<double, double, double, double, double, double>(),
           nb::arg("mean") = 0.0, nb::arg("trend") = 0.0,
           nb::arg("amplitude") = 0.0, nb::arg("phase") = 0.0,
           nb::arg("phi") = 0.0, nb::arg("sigma") = 0.0)
      .def_rw("mean", &TemperatureModel::mean)
      .def_rw("trend", &TemperatureModel::trend)
      .def_rw("amplitude", &TemperatureModel::amplitude)
      .def_rw("phase", &TemperatureModel::phase)
      .def_rw("phi", &TemperatureModel::phi)
      .def_rw("sigma", &TemperatureModel::sigma)
      .def("seasonal", &TemperatureModel::Seasonal, nb::arg("day"));
  m.def(
      "fit_temperature_model",
      [](const std::vector<double>& temperatures, int first_day) {
        return Unwrap(p::FitTemperatureModel(temperatures, first_day),
                      "need at least 60 observations");
      },
      nb::arg("temperatures"), nb::arg("first_day") = 0);
  using p::SeasonSpec;
  nb::class_<SeasonSpec>(m, "SeasonSpec")
      .def(nb::init<p::DegreeDayKind, int, int, double, double>(),
           nb::arg("kind") = p::DegreeDayKind::kHeating,
           nb::arg("first_day") = 0, nb::arg("days") = 0,
           nb::arg("last_deviation") = 0.0, nb::arg("base") = 65.0)
      .def_rw("kind", &SeasonSpec::kind)
      .def_rw("first_day", &SeasonSpec::first_day)
      .def_rw("days", &SeasonSpec::days)
      .def_rw("last_deviation", &SeasonSpec::last_deviation)
      .def_rw("base", &SeasonSpec::base);
  using p::DegreeDaySimulation;
  nb::class_<DegreeDaySimulation>(m, "DegreeDaySimulation")
      .def(nb::init<ExpectationEstimate, double, double>(), nb::arg("option"),
           nb::arg("mean_degree_days"), nb::arg("std_degree_days"))
      .def_rw("option", &DegreeDaySimulation::option)
      .def_rw("mean_degree_days", &DegreeDaySimulation::mean_degree_days)
      .def_rw("std_degree_days", &DegreeDaySimulation::std_degree_days);
  m.def(
      "simulate_degree_day_option",
      [](const TemperatureModel& model, const SeasonSpec& season,
         const DegreeDayOption& option, double rate, double maturity, int paths,
         std::uint64_t seed) {
        return Unwrap(p::SimulateDegreeDayOption(model, season, option, rate,
                                                 maturity, paths, seed),
                      "invalid simulation inputs");
      },
      nb::arg("model"), nb::arg("season"), nb::arg("option"), nb::arg("rate"),
      nb::arg("maturity"), nb::arg("paths"), nb::arg("seed"));
}

void BindInsurance(nb::module_& m) {
  namespace p = pricers;
  using p::LossModel;
  nb::class_<LossModel>(m, "LossModel")
      .def(nb::init<double, double, double>(), nb::arg("intensity") = 0.0,
           nb::arg("severity_mu") = 0.0, nb::arg("severity_sigma") = 0.0)
      .def_rw("intensity", &LossModel::intensity)
      .def_rw("severity_mu", &LossModel::severity_mu)
      .def_rw("severity_sigma", &LossModel::severity_sigma);
  using p::CatBondTerms;
  nb::class_<CatBondTerms>(m, "CatBondTerms")
      .def(nb::init<double, double, double, double, double, int>(),
           nb::arg("principal") = 100.0, nb::arg("coupon") = 0.0,
           nb::arg("maturity") = 1.0, nb::arg("attachment") = 0.0,
           nb::arg("exhaustion") = 0.0, nb::arg("frequency") = 1)
      .def_rw("principal", &CatBondTerms::principal)
      .def_rw("coupon", &CatBondTerms::coupon)
      .def_rw("maturity", &CatBondTerms::maturity)
      .def_rw("attachment", &CatBondTerms::attachment)
      .def_rw("exhaustion", &CatBondTerms::exhaustion)
      .def_rw("frequency", &CatBondTerms::frequency);
  using p::CatBondValue;
  nb::class_<CatBondValue>(m, "CatBondValue")
      .def(nb::init<double, double, double, double, double>(), nb::arg("price"),
           nb::arg("std_error"), nb::arg("expected_loss"),
           nb::arg("loss_probability"), nb::arg("spread"))
      .def_rw("price", &CatBondValue::price)
      .def_rw("std_error", &CatBondValue::std_error)
      .def_rw("expected_loss", &CatBondValue::expected_loss)
      .def_rw("loss_probability", &CatBondValue::loss_probability)
      .def_rw("spread", &CatBondValue::spread);
  m.def(
      "price_cat_bond",
      [](const CatBondTerms& terms, const LossModel& losses, double rate,
         int paths, std::uint64_t seed) {
        return Unwrap(p::PriceCatBond(terms, losses, rate, paths, seed),
                      "invalid cat bond inputs");
      },
      nb::arg("terms"), nb::arg("losses"), nb::arg("rate"), nb::arg("paths"),
      nb::arg("seed"));
  m.def(
      "excess_of_loss_layer",
      [](const LossModel& losses, double attachment, double limit, double years,
         double rate, int paths, std::uint64_t seed) {
        return Unwrap(p::ExcessOfLossLayer(losses, attachment, limit, years,
                                           rate, paths, seed),
                      "invalid layer inputs");
      },
      nb::arg("losses"), nb::arg("attachment"), nb::arg("limit"),
      nb::arg("years"), nb::arg("rate"), nb::arg("paths"), nb::arg("seed"));
}

void BindRealOptions(nb::module_& m) {
  namespace p = pricers;
  m.def("market_price_of_risk", &p::MarketPriceOfRisk, nb::arg("correlation"),
        nb::arg("market_return"), nb::arg("rate"), nb::arg("market_vol"));
  m.def("risk_neutral_growth", &p::RiskNeutralGrowth, nb::arg("growth"),
        nb::arg("lambda_"), nb::arg("volatility"));
  m.def(
      "risk_neutral_present_value",
      [](const std::vector<double>& times, const std::vector<double>& expected,
         double lambda, double volatility, double rate) {
        return Unwrap(p::RiskNeutralPresentValue(times, expected, lambda,
                                                 volatility, rate),
                      "times and expected must match");
      },
      nb::arg("times"), nb::arg("expected"), nb::arg("lambda_"),
      nb::arg("volatility"), nb::arg("rate"));
  nb::class_<p::Abandon>(m, "Abandon")
      .def(nb::init<double>(), nb::arg("salvage"))
      .def_rw("salvage", &p::Abandon::salvage);
  nb::class_<p::Expand>(m, "Expand")
      .def(nb::init<double, double>(), nb::arg("factor"), nb::arg("cost"))
      .def_rw("factor", &p::Expand::factor)
      .def_rw("cost", &p::Expand::cost);
  nb::class_<p::Contract>(m, "Contract")
      .def(nb::init<double, double>(), nb::arg("factor"), nb::arg("saving"))
      .def_rw("factor", &p::Contract::factor)
      .def_rw("saving", &p::Contract::saving);
  m.def("exercise_value", &p::ExerciseValue, nb::arg("option"),
        nb::arg("value"));
  using p::ProjectLattice;
  nb::class_<ProjectLattice>(m, "ProjectLattice")
      .def(nb::init<double, double, double, double, double, int>(),
           nb::arg("value") = 0.0, nb::arg("volatility") = 0.0,
           nb::arg("rate") = 0.0, nb::arg("payout") = 0.0,
           nb::arg("maturity") = 0.0, nb::arg("steps") = 0)
      .def_rw("value", &ProjectLattice::value)
      .def_rw("volatility", &ProjectLattice::volatility)
      .def_rw("rate", &ProjectLattice::rate)
      .def_rw("payout", &ProjectLattice::payout)
      .def_rw("maturity", &ProjectLattice::maturity)
      .def_rw("steps", &ProjectLattice::steps);
  m.def(
      "project_with_options",
      [](const ProjectLattice& lattice,
         const std::vector<p::ProjectOption>& options) {
        return Unwrap(p::ProjectWithOptions(lattice, options),
                      "invalid lattice");
      },
      nb::arg("lattice"), nb::arg("options"));
  m.def(
      "deferral_option",
      [](const ProjectLattice& lattice, double cost) {
        return Unwrap(p::DeferralOption(lattice, cost), "invalid lattice");
      },
      nb::arg("lattice"), nb::arg("cost"));
  using p::ExtractionProject;
  nb::class_<ExtractionProject>(m, "ExtractionProject")
      .def(nb::init<double, double, double, double, double, double, double>(),
           nb::arg("production") = 0.0, nb::arg("variable_cost") = 0.0,
           nb::arg("fixed_cost") = 0.0, nb::arg("rate") = 0.0,
           nb::arg("abandon_value") = 0.0, nb::arg("expand_factor") = 1.0,
           nb::arg("expand_cost") = 0.0)
      .def_rw("production", &ExtractionProject::production)
      .def_rw("variable_cost", &ExtractionProject::variable_cost)
      .def_rw("fixed_cost", &ExtractionProject::fixed_cost)
      .def_rw("rate", &ExtractionProject::rate)
      .def_rw("abandon_value", &ExtractionProject::abandon_value)
      .def_rw("expand_factor", &ExtractionProject::expand_factor)
      .def_rw("expand_cost", &ExtractionProject::expand_cost);
  using p::ProjectValuation;
  nb::class_<ProjectValuation>(m, "ProjectValuation")
      .def(nb::init<double, double, double>(), nb::arg("base"),
           nb::arg("with_abandon"), nb::arg("with_expand"))
      .def_rw("base", &ProjectValuation::base)
      .def_rw("with_abandon", &ProjectValuation::with_abandon)
      .def_rw("with_expand", &ProjectValuation::with_expand);
  m.def(
      "value_extraction_project",
      [](const p::CommodityTree& tree, const ExtractionProject& project) {
        return Unwrap(p::ValueExtractionProject(tree, project),
                      "invalid project");
      },
      nb::arg("tree"), nb::arg("project"));
  using p::BusinessSpec;
  nb::class_<BusinessSpec>(m, "BusinessSpec")
      .def(nb::init<double, double, double, double, double, double, double,
                    double, int, double>(),
           nb::arg("revenue") = 0.0, nb::arg("growth") = 0.0,
           nb::arg("volatility") = 0.0, nb::arg("variable_cost") = 0.0,
           nb::arg("fixed_cost") = 0.0, nb::arg("cash") = 0.0,
           nb::arg("rate") = 0.0, nb::arg("years") = 0.0,
           nb::arg("steps_per_year") = 4, nb::arg("terminal_multiple") = 0.0)
      .def_rw("revenue", &BusinessSpec::revenue)
      .def_rw("growth", &BusinessSpec::growth)
      .def_rw("volatility", &BusinessSpec::volatility)
      .def_rw("variable_cost", &BusinessSpec::variable_cost)
      .def_rw("fixed_cost", &BusinessSpec::fixed_cost)
      .def_rw("cash", &BusinessSpec::cash)
      .def_rw("rate", &BusinessSpec::rate)
      .def_rw("years", &BusinessSpec::years)
      .def_rw("steps_per_year", &BusinessSpec::steps_per_year)
      .def_rw("terminal_multiple", &BusinessSpec::terminal_multiple);
  using p::BusinessValue;
  nb::class_<BusinessValue>(m, "BusinessValue")
      .def(nb::init<double, double, double>(), nb::arg("value"),
           nb::arg("std_error"), nb::arg("bankruptcy_probability"))
      .def_rw("value", &BusinessValue::value)
      .def_rw("std_error", &BusinessValue::std_error)
      .def_rw("bankruptcy_probability", &BusinessValue::bankruptcy_probability);
  m.def(
      "value_business",
      [](const BusinessSpec& spec, int paths, std::uint64_t seed) {
        return Unwrap(p::ValueBusiness(spec, paths, seed),
                      "invalid business inputs");
      },
      nb::arg("spec"), nb::arg("paths"), nb::arg("seed"));
}

}  // namespace

void BindCommodities(nb::module_& pricers, nb::module_& models) {
  BindCommodityModels(models);
  BindCommodityTree(pricers);
  BindWeather(pricers);
  BindInsurance(pricers);
  BindRealOptions(pricers);
}

}  // namespace ofod::bindings
