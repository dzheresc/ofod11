// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <vector>

#include "bindings.h"
#include "ofod/models/brownian.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindModels(nb::module_& m) {
  using models::ArithmeticBrownianMotion;
  using models::GeometricBrownianMotion;
  using models::Interval;
  using Shocks = const std::vector<double>&;

  nb::class_<Interval>(m, "Interval")
      .def(nb::init<double, double>(), nb::arg("lower"), nb::arg("upper"))
      .def_rw("lower", &Interval::lower)
      .def_rw("upper", &Interval::upper);

  nb::class_<ArithmeticBrownianMotion>(m, "ArithmeticBrownianMotion")
      .def(
          "__init__",
          [](ArithmeticBrownianMotion* self, double drift, double volatility) {
            new (self) ArithmeticBrownianMotion{drift, volatility};
          },
          nb::arg("drift") = 0.0, nb::arg("volatility") = 1.0)
      .def_rw("drift", &ArithmeticBrownianMotion::drift)
      .def_rw("volatility", &ArithmeticBrownianMotion::volatility)
      .def(
          "path",
          [](const ArithmeticBrownianMotion& w, double x0, double dt,
             Shocks shocks) { return w.Path(x0, dt, shocks); },
          nb::arg("x0"), nb::arg("dt"), nb::arg("shocks"));

  nb::class_<GeometricBrownianMotion>(m, "GeometricBrownianMotion")
      .def(
          "__init__",
          [](GeometricBrownianMotion* self, double drift, double volatility) {
            new (self) GeometricBrownianMotion{drift, volatility};
          },
          nb::arg("drift") = 0.0, nb::arg("volatility") = 0.0)
      .def_rw("drift", &GeometricBrownianMotion::drift)
      .def_rw("volatility", &GeometricBrownianMotion::volatility)
      .def(
          "euler_path",
          [](const GeometricBrownianMotion& g, double s0, double dt,
             Shocks shocks) { return g.EulerPath(s0, dt, shocks); },
          nb::arg("s0"), nb::arg("dt"), nb::arg("shocks"))
      .def(
          "exact_path",
          [](const GeometricBrownianMotion& g, double s0, double dt,
             Shocks shocks) { return g.ExactPath(s0, dt, shocks); },
          nb::arg("s0"), nb::arg("dt"), nb::arg("shocks"))
      .def("expected_value", &GeometricBrownianMotion::ExpectedValue,
           nb::arg("s0"), nb::arg("t"))
      .def("variance", &GeometricBrownianMotion::Variance, nb::arg("s0"),
           nb::arg("t"))
      .def("log_mean", &GeometricBrownianMotion::LogMean, nb::arg("s0"),
           nb::arg("t"))
      .def("log_std_dev", &GeometricBrownianMotion::LogStdDev, nb::arg("t"))
      .def("confidence_interval", &GeometricBrownianMotion::ConfidenceInterval,
           nb::arg("s0"), nb::arg("t"), nb::arg("z"));

  m.def(
      "quadratic_variation",
      [](const std::vector<double>& path) {
        return models::QuadraticVariation(path);
      },
      nb::arg("path"));
}

}  // namespace ofod::bindings
