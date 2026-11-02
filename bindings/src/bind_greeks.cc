// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <vector>

#include "bindings.h"
#include "ofod/greeks/greeks.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindGreeks(nb::module_& m) {
  using greeks::BumpSizes;
  using greeks::Exposure;
  using greeks::Greeks;
  using greeks::HedgeExperiment;
  using greeks::HedgePerformance;
  using greeks::NeutralHedge;
  using greeks::OptionInputs;

  nb::class_<OptionInputs>(m, "OptionInputs")
      .def(
          "__init__",
          [](OptionInputs* self, double spot, double rate, double yield,
             double volatility, double maturity) {
            new (self) OptionInputs{spot, rate, yield, volatility, maturity};
          },
          nb::arg("spot") = 0.0, nb::arg("rate") = 0.0, nb::arg("yield_") = 0.0,
          nb::arg("volatility") = 0.0, nb::arg("maturity") = 0.0)
      .def_rw("spot", &OptionInputs::spot)
      .def_rw("rate", &OptionInputs::rate)
      .def_rw("yield_", &OptionInputs::yield)
      .def_rw("volatility", &OptionInputs::volatility)
      .def_rw("maturity", &OptionInputs::maturity);

  nb::class_<Greeks>(m, "Greeks")
      .def(nb::init<double, double, double, double, double>(), nb::arg("delta"),
           nb::arg("gamma"), nb::arg("vega"), nb::arg("theta"), nb::arg("rho"))
      .def_rw("delta", &Greeks::delta)
      .def_rw("gamma", &Greeks::gamma)
      .def_rw("vega", &Greeks::vega)
      .def_rw("theta", &Greeks::theta)
      .def_rw("rho", &Greeks::rho);

  m.def("bsm_greeks", &greeks::BsmGreeks, nb::arg("option_type"),
        nb::arg("strike"), nb::arg("inputs"));

  nb::class_<BumpSizes>(m, "BumpSizes")
      .def(
          "__init__",
          [](BumpSizes* self, double spot, double volatility, double rate,
             double time) {
            new (self) BumpSizes{spot, volatility, rate, time};
          },
          nb::arg("spot") = 1e-3, nb::arg("volatility") = 1e-4,
          nb::arg("rate") = 1e-4, nb::arg("time") = 1.0 / 365.0)
      .def_rw("spot", &BumpSizes::spot)
      .def_rw("volatility", &BumpSizes::volatility)
      .def_rw("rate", &BumpSizes::rate)
      .def_rw("time", &BumpSizes::time);

  m.def(
      "bump_greeks",
      [](const greeks::Pricer& pricer, const OptionInputs& inputs,
         const BumpSizes* bumps) {
        return greeks::BumpGreeksErased(pricer, inputs,
                                        bumps ? *bumps : BumpSizes{});
      },
      nb::arg("pricer"), nb::arg("inputs"),
      nb::arg("bumps").none() = nb::none());

  nb::class_<Exposure>(m, "Exposure")
      .def(
          "__init__",
          [](Exposure* self, double delta, double gamma, double vega) {
            new (self) Exposure{delta, gamma, vega};
          },
          nb::arg("delta") = 0.0, nb::arg("gamma") = 0.0, nb::arg("vega") = 0.0)
      .def_rw("delta", &Exposure::delta)
      .def_rw("gamma", &Exposure::gamma)
      .def_rw("vega", &Exposure::vega);

  nb::class_<NeutralHedge>(m, "NeutralHedge")
      .def(nb::init<double, double, double>(), nb::arg("option1"),
           nb::arg("option2"), nb::arg("underlying"))
      .def_rw("option1", &NeutralHedge::option1)
      .def_rw("option2", &NeutralHedge::option2)
      .def_rw("underlying", &NeutralHedge::underlying);

  m.def(
      "gamma_neutral_hedge",
      [](const Exposure& portfolio, const Exposure& option) {
        const auto h = greeks::GammaNeutralHedge(portfolio, option);
        if (!h) throw nb::value_error("the hedging option has zero gamma");
        return *h;
      },
      nb::arg("portfolio"), nb::arg("option"));
  m.def(
      "gamma_vega_neutral_hedge",
      [](const Exposure& portfolio, const Exposure& option1,
         const Exposure& option2) {
        const auto h =
            greeks::GammaVegaNeutralHedge(portfolio, option1, option2);
        if (!h) {
          throw nb::value_error("the hedging options are linearly dependent");
        }
        return *h;
      },
      nb::arg("portfolio"), nb::arg("option1"), nb::arg("option2"));

  m.def(
      "scenario_grid",
      [](const greeks::Pricer& pricer, const OptionInputs& base,
         const std::vector<double>& spots,
         const std::vector<double>& volatilities) {
        return greeks::ScenarioGrid(pricer, base, spots, volatilities);
      },
      nb::arg("pricer"), nb::arg("base"), nb::arg("spots"),
      nb::arg("volatilities"));

  nb::class_<HedgeExperiment>(m, "HedgeExperiment")
      .def(
          "__init__",
          [](HedgeExperiment* self, double spot, double strike, double rate,
             double drift, double volatility, double maturity) {
            new (self) HedgeExperiment{spot,  strike,     rate,
                                       drift, volatility, maturity};
          },
          nb::arg("spot") = 0.0, nb::arg("strike") = 0.0, nb::arg("rate") = 0.0,
          nb::arg("drift") = 0.0, nb::arg("volatility") = 0.0,
          nb::arg("maturity") = 0.0)
      .def_rw("spot", &HedgeExperiment::spot)
      .def_rw("strike", &HedgeExperiment::strike)
      .def_rw("rate", &HedgeExperiment::rate)
      .def_rw("drift", &HedgeExperiment::drift)
      .def_rw("volatility", &HedgeExperiment::volatility)
      .def_rw("maturity", &HedgeExperiment::maturity);

  nb::class_<HedgePerformance>(m, "HedgePerformance")
      .def(nb::init<double, double, double, double>(), nb::arg("mean_cost"),
           nb::arg("std_cost"), nb::arg("bsm_price"), nb::arg("ratio"))
      .def_rw("mean_cost", &HedgePerformance::mean_cost)
      .def_rw("std_cost", &HedgePerformance::std_cost)
      .def_rw("bsm_price", &HedgePerformance::bsm_price)
      .def_rw("ratio", &HedgePerformance::ratio);

  m.def(
      "simulate_delta_hedge",
      [](const HedgeExperiment& experiment, int rebalances, int paths,
         std::uint64_t seed) {
        const auto r =
            greeks::SimulateDeltaHedge(experiment, rebalances, paths, seed);
        if (!r) throw nb::value_error("need rebalances >= 1 and paths >= 2");
        return *r;
      },
      nb::arg("experiment"), nb::arg("rebalances"), nb::arg("paths"),
      nb::arg("seed"));
}

}  // namespace ofod::bindings
