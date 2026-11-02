// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/vector.h>

#include <optional>
#include <utility>
#include <vector>

#include "bindings.h"
#include "ofod/curves/bond_bootstrap.h"
#include "ofod/curves/credit_curve.h"
#include "ofod/curves/swap_bootstrap.h"
#include "ofod/curves/vol_surface.h"
#include "ofod/curves/zero_curve.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindCurves(nb::module_& m) {
  using curves::BondQuote;
  using curves::ZeroCurve;

  nb::class_<ZeroCurve>(m, "ZeroCurve")
      .def(
          "__init__",
          [](ZeroCurve* self, std::vector<double> times,
             std::vector<double> zero_rates) {
            auto curve =
                ZeroCurve::Create(std::move(times), std::move(zero_rates));
            if (!curve) {
              throw nb::value_error(
                  "times must be non-empty, strictly increasing and match "
                  "zero_rates");
            }
            new (self) ZeroCurve(*std::move(curve));
          },
          nb::arg("times"), nb::arg("zero_rates"))
      .def_static("flat", &ZeroCurve::Flat, nb::arg("zero_rate"))
      .def("zero_rate", &ZeroCurve::ZeroRate, nb::arg("t"))
      .def("discount_factor", &ZeroCurve::DiscountFactor, nb::arg("t"))
      .def("forward_rate", &ZeroCurve::ForwardRate, nb::arg("t1"),
           nb::arg("t2"))
      .def_prop_ro("times",
                   [](const ZeroCurve& c) {
                     return std::vector<double>(c.times().begin(),
                                                c.times().end());
                   })
      .def_prop_ro("zero_rates", [](const ZeroCurve& c) {
        return std::vector<double>(c.zero_rates().begin(),
                                   c.zero_rates().end());
      });

  using curves::VolInterpolation;
  using curves::VolSurface;
  nb::enum_<VolInterpolation>(m, "VolInterpolation")
      .value("TOTAL_VARIANCE", VolInterpolation::kTotalVariance)
      .value("LINEAR_VOL", VolInterpolation::kLinearVol);
  nb::class_<VolSurface>(m, "VolSurface")
      .def(
          "__init__",
          [](VolSurface* self, std::vector<double> maturities,
             std::vector<double> strikes,
             std::vector<std::vector<double>> volatilities,
             VolInterpolation interpolation) {
            auto surface =
                VolSurface::Create(std::move(maturities), std::move(strikes),
                                   std::move(volatilities), interpolation);
            if (!surface) {
              throw nb::value_error("invalid volatility surface grid");
            }
            new (self) VolSurface(*std::move(surface));
          },
          nb::arg("maturities"), nb::arg("strikes"), nb::arg("volatilities"),
          nb::arg("interpolation") = VolInterpolation::kTotalVariance)
      .def("volatility", &VolSurface::Volatility, nb::arg("strike"),
           nb::arg("t"))
      .def_prop_ro("maturities",
                   [](const VolSurface& s) {
                     return std::vector<double>(s.maturities().begin(),
                                                s.maturities().end());
                   })
      .def_prop_ro("strikes",
                   [](const VolSurface& s) {
                     return std::vector<double>(s.strikes().begin(),
                                                s.strikes().end());
                   })
      .def_prop_ro("volatilities", &VolSurface::volatilities)
      .def_prop_ro("interpolation", &VolSurface::interpolation);

  nb::class_<BondQuote>(m, "BondQuote")
      .def(nb::init<instruments::Bond, double>(), nb::arg("bond"),
           nb::arg("price"))
      .def_rw("bond", &BondQuote::bond)
      .def_rw("price", &BondQuote::price);

  m.def(
      "bootstrap_from_bonds",
      [](const std::vector<BondQuote>& quotes) {
        auto curve = curves::BootstrapFromBonds(quotes);
        if (!curve) {
          throw nb::value_error(
              "bond maturities must be distinct and every bond repriceable");
        }
        return *std::move(curve);
      },
      nb::arg("quotes"));

  using curves::SwapQuote;
  nb::class_<SwapQuote>(m, "SwapQuote")
      .def(
          "__init__",
          [](SwapQuote* self, double maturity, double par_rate, int frequency) {
            new (self) SwapQuote{maturity, par_rate, frequency};
          },
          nb::arg("maturity") = 0.0, nb::arg("par_rate") = 0.0,
          nb::arg("frequency") = 1)
      .def_rw("maturity", &SwapQuote::maturity)
      .def_rw("par_rate", &SwapQuote::par_rate)
      .def_rw("frequency", &SwapQuote::frequency);
  m.def(
      "bootstrap_from_swaps",
      [](const std::vector<SwapQuote>& quotes) {
        auto curve = curves::BootstrapFromSwaps(quotes);
        if (!curve) {
          throw nb::value_error(
              "swap maturities must be distinct and every swap repriceable");
        }
        return *std::move(curve);
      },
      nb::arg("quotes"));

  // Chapter 24: hazard-rate credit curves.
  using curves::CreditCurve;
  nb::class_<CreditCurve>(m, "CreditCurve")
      .def(
          "__init__",
          [](CreditCurve* self, std::vector<double> times,
             std::vector<double> hazards) {
            auto curve =
                CreditCurve::Create(std::move(times), std::move(hazards));
            if (!curve) throw nb::value_error("invalid credit curve");
            new (self) CreditCurve(*std::move(curve));
          },
          nb::arg("times"), nb::arg("hazards"))
      .def_static("flat", &CreditCurve::Flat, nb::arg("hazard"))
      .def("survival_probability", &CreditCurve::SurvivalProbability,
           nb::arg("t"))
      .def(
          "default_probability",
          [](const CreditCurve& c, double t1, std::optional<double> t2) {
            return t2 ? c.DefaultProbability(t1, *t2)
                      : c.DefaultProbability(t1);
          },
          nb::arg("t1"), nb::arg("t2") = nb::none())
      .def("hazard", &CreditCurve::Hazard, nb::arg("t"))
      .def_prop_ro("times",
                   [](const CreditCurve& c) {
                     return std::vector<double>(c.times().begin(),
                                                c.times().end());
                   })
      .def_prop_ro("hazards", [](const CreditCurve& c) {
        return std::vector<double>(c.hazards().begin(), c.hazards().end());
      });
  m.def("hazard_from_spread", &curves::HazardFromSpread, nb::arg("spread"),
        nb::arg("recovery"));
  m.def("average_hazard", &curves::AverageHazard, nb::arg("cumulative_default"),
        nb::arg("t"));
}

}  // namespace ofod::bindings
