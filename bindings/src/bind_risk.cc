// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <vector>

#include "bindings.h"
#include "ofod/risk/hedging.h"

namespace ofod::bindings {

namespace nb = nanobind;

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
}

}  // namespace ofod::bindings
