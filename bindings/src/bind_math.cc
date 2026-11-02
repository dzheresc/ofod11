// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/vector.h>

#include <functional>
#include <optional>
#include <vector>

#include "bindings.h"
#include "ofod/math/interpolation.h"
#include "ofod/math/normal.h"
#include "ofod/math/roots.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindMath(nb::module_& m) {
  using Fn = std::function<double(double)>;

  m.def("norm_pdf", &math::NormPdf, nb::arg("x"));
  m.def("norm_cdf", &math::NormCdf, nb::arg("x"));
  m.def("norm_cdf_poly", &math::NormCdfPoly, nb::arg("x"));

  m.def(
      "brent",
      [](const Fn& f, double lo, double hi, double tolerance,
         int max_iterations) {
        return math::Brent(f, lo, hi, {tolerance, max_iterations});
      },
      nb::arg("f"), nb::arg("lo"), nb::arg("hi"), nb::arg("tolerance") = 1e-12,
      nb::arg("max_iterations") = 200);
  m.def(
      "newton",
      [](const Fn& f, const Fn& df, double x0, double tolerance,
         int max_iterations) {
        return math::Newton(f, df, x0, {tolerance, max_iterations});
      },
      nb::arg("f"), nb::arg("df"), nb::arg("x0"), nb::arg("tolerance") = 1e-12,
      nb::arg("max_iterations") = 200);

  nb::class_<math::LinearInterpolator>(m, "LinearInterpolator")
      .def(
          "__init__",
          [](math::LinearInterpolator* self, std::vector<double> xs,
             std::vector<double> ys) {
            auto f =
                math::LinearInterpolator::Create(std::move(xs), std::move(ys));
            if (!f) {
              throw nb::value_error(
                  "xs and ys must be non-empty, equal length and xs strictly "
                  "increasing");
            }
            new (self) math::LinearInterpolator(*std::move(f));
          },
          nb::arg("xs"), nb::arg("ys"))
      .def("__call__", &math::LinearInterpolator::operator(), nb::arg("x"))
      .def_prop_ro("xs",
                   [](const math::LinearInterpolator& f) {
                     return std::vector<double>(f.xs().begin(), f.xs().end());
                   })
      .def_prop_ro("ys", [](const math::LinearInterpolator& f) {
        return std::vector<double>(f.ys().begin(), f.ys().end());
      });
}

}  // namespace ofod::bindings
