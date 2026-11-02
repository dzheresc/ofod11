// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

#include "bindings.h"
#include "ofod/risk/hedging.h"
#include "ofod/risk/var.h"

namespace ofod::bindings {

namespace nb = nanobind;

namespace {

constexpr const char* kTooFew = "too few scenarios for this confidence level";

}  // namespace

void BindRisk(nb::module_& m) {
  using risk::RollLeg;

  m.def("minimum_variance_hedge_ratio", &risk::MinimumVarianceHedgeRatio,
        nb::arg("correlation"), nb::arg("spot_vol"), nb::arg("futures_vol"));
  m.def(
      "hedge_ratio_from_changes",
      [](const std::vector<double>& spot_changes,
         const std::vector<double>& futures_changes) {
        const auto h =
            risk::HedgeRatioFromChanges(spot_changes, futures_changes);
        if (!h) {
          throw nb::value_error(
              "need equal samples and non-constant futures changes");
        }
        return *h;
      },
      nb::arg("spot_changes"), nb::arg("futures_changes"));
  m.def("hedge_effectiveness", &risk::HedgeEffectiveness,
        nb::arg("correlation"));
  m.def("optimal_contracts", &risk::OptimalContracts, nb::arg("hedge_ratio"),
        nb::arg("exposure_units"), nb::arg("contract_units"));
  m.def("tailed_contracts", &risk::TailedContracts, nb::arg("hedge_ratio"),
        nb::arg("exposure_value"), nb::arg("futures_value"));
  m.def("beta_hedge_contracts", &risk::BetaHedgeContracts, nb::arg("beta"),
        nb::arg("portfolio_value"), nb::arg("futures_value"));
  m.def("change_beta_contracts", &risk::ChangeBetaContracts, nb::arg("beta"),
        nb::arg("target_beta"), nb::arg("portfolio_value"),
        nb::arg("futures_value"));
  m.def("basis", &risk::Basis, nb::arg("spot"), nb::arg("futures"));
  m.def("effective_hedge_price", &risk::EffectiveHedgePrice,
        nb::arg("spot_at_close"), nb::arg("futures_at_open"),
        nb::arg("futures_at_close"));

  nb::class_<RollLeg>(m, "RollLeg")
      .def(nb::init<double, double>(), nb::arg("open_price"),
           nb::arg("close_price"))
      .def_rw("open_price", &RollLeg::open_price)
      .def_rw("close_price", &RollLeg::close_price);
  m.def(
      "short_roll_gain",
      [](const std::vector<RollLeg>& legs) {
        return risk::ShortRollGain(legs);
      },
      nb::arg("legs"));

  // Chapter 22: value at risk and expected shortfall.
  m.def("normal_var", &risk::NormalVar, nb::arg("sd"), nb::arg("confidence"),
        nb::arg("mean") = 0.0);
  m.def("normal_expected_shortfall", &risk::NormalExpectedShortfall,
        nb::arg("sd"), nb::arg("confidence"), nb::arg("mean") = 0.0);
  m.def("scale_to_horizon", &risk::ScaleToHorizon, nb::arg("one_day"),
        nb::arg("days"));
  m.def(
      "historical_var",
      [](const std::vector<double>& losses, double confidence) {
        const auto v = risk::HistoricalVar(losses, confidence);
        if (!v) throw nb::value_error(kTooFew);
        return *v;
      },
      nb::arg("losses"), nb::arg("confidence"));
  m.def(
      "historical_expected_shortfall",
      [](const std::vector<double>& losses, double confidence) {
        const auto v = risk::HistoricalExpectedShortfall(losses, confidence);
        if (!v) throw nb::value_error(kTooFew);
        return *v;
      },
      nb::arg("losses"), nb::arg("confidence"));
  m.def(
      "volatility_adjusted_changes",
      [](const std::vector<double>& changes, const std::vector<double>& vols,
         double current_vol) {
        auto v = risk::VolatilityAdjustedChanges(changes, vols, current_vol);
        if (!v) {
          throw nb::value_error(
              "changes and vols must match and vols be positive");
        }
        return *std::move(v);
      },
      nb::arg("changes"), nb::arg("vols"), nb::arg("current_vol"));
  m.def(
      "portfolio_std_dev",
      [](const std::vector<double>& exposures, const std::vector<double>& vols,
         const math::Matrix& correlation) {
        const auto sd = risk::PortfolioStdDev(exposures, vols, correlation);
        if (!sd) throw nb::value_error("inconsistent sizes");
        return *sd;
      },
      nb::arg("exposures"), nb::arg("vols"), nb::arg("correlation"));

  using risk::Moments;
  nb::class_<Moments>(m, "Moments")
      .def(nb::init<double, double, double>(), nb::arg("mean"),
           nb::arg("std_dev"), nb::arg("skewness"))
      .def_rw("mean", &Moments::mean)
      .def_rw("std_dev", &Moments::std_dev)
      .def_rw("skewness", &Moments::skewness);
  m.def("delta_gamma_moments", &risk::DeltaGammaMoments, nb::arg("delta"),
        nb::arg("gamma"), nb::arg("sd_change"));
  m.def("cornish_fisher_var", &risk::CornishFisherVar, nb::arg("pnl"),
        nb::arg("confidence"));

  using risk::VarEstimate;
  nb::class_<VarEstimate>(m, "VarEstimate")
      .def(nb::init<double, double>(), nb::arg("var"),
           nb::arg("expected_shortfall"))
      .def_rw("var", &VarEstimate::var)
      .def_rw("expected_shortfall", &VarEstimate::expected_shortfall);
  m.def(
      "monte_carlo_var",
      [](const std::function<double(std::vector<double>)>& pnl,
         const math::Matrix& covariance, double confidence, int paths,
         std::uint64_t seed) {
        const risk::PnlFunction f = [&pnl](std::span<const double> dx) {
          return pnl(std::vector<double>(dx.begin(), dx.end()));
        };
        const auto v =
            risk::MonteCarloVar(f, covariance, confidence, paths, seed);
        if (!v) {
          throw nb::value_error(
              "covariance must be positive definite and paths enough for "
              "the confidence level");
        }
        return *v;
      },
      nb::arg("pnl"), nb::arg("covariance"), nb::arg("confidence"),
      nb::arg("paths"), nb::arg("seed"));
  m.def("kupiec_statistic", &risk::KupiecStatistic, nb::arg("observations"),
        nb::arg("exceptions"), nb::arg("p"));
  m.def("binomial_tail_probability", &risk::BinomialTailProbability,
        nb::arg("n"), nb::arg("m"), nb::arg("p"));

  using risk::PcaResult;
  nb::class_<PcaResult>(m, "PcaResult")
      .def(nb::init<std::vector<double>, math::Matrix>(), nb::arg("variances"),
           nb::arg("loadings"))
      .def_rw("variances", &PcaResult::variances)
      .def_rw("loadings", &PcaResult::loadings);
  m.def(
      "principal_components",
      [](const math::Matrix& observations) {
        auto r = risk::PrincipalComponents(observations);
        if (!r) {
          throw nb::value_error(
              "need at least two non-empty rows of equal length");
        }
        return *std::move(r);
      },
      nb::arg("observations"));
  m.def(
      "pca_std_dev",
      [](const std::vector<double>& exposures, const math::Matrix& loadings,
         const std::vector<double>& factor_sds) {
        const auto sd = risk::PcaStdDev(exposures, loadings, factor_sds);
        if (!sd) throw nb::value_error("inconsistent sizes");
        return *sd;
      },
      nb::arg("exposures"), nb::arg("loadings"), nb::arg("factor_sds"));
}

}  // namespace ofod::bindings
