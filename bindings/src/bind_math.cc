// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/vector.h>

#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "bindings.h"
#include "ofod/math/interpolation.h"
#include "ofod/math/linalg.h"
#include "ofod/math/normal.h"
#include "ofod/math/optimize.h"
#include "ofod/math/roots.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindMath(nb::module_& m) {
  using Fn = std::function<double(double)>;

  m.def("norm_pdf", &math::NormPdf, nb::arg("x"));
  m.def("norm_cdf", &math::NormCdf, nb::arg("x"));
  m.def("norm_cdf_poly", &math::NormCdfPoly, nb::arg("x"));
  m.def("norm_inv", &math::NormInv, nb::arg("p"));

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

  // Chapter 22-23 numerics.
  nb::class_<math::Eigen>(m, "Eigen")
      .def(nb::init<std::vector<double>, math::Matrix>(), nb::arg("values"),
           nb::arg("vectors"))
      .def_rw("values", &math::Eigen::values)
      .def_rw("vectors", &math::Eigen::vectors);
  m.def(
      "symmetric_eigen",
      [](const math::Matrix& a) {
        auto e = math::SymmetricEigen(a);
        if (!e) {
          throw nb::value_error(
              "matrix must be square, non-empty and symmetric, and the "
              "sweeps must converge");
        }
        return *std::move(e);
      },
      nb::arg("matrix"));

  using math::Minimum;
  using math::NelderMeadOptions;
  nb::class_<Minimum>(m, "Minimum")
      .def(nb::init<std::vector<double>, double, int, bool>(), nb::arg("x"),
           nb::arg("value"), nb::arg("iterations"), nb::arg("converged"))
      .def_rw("x", &Minimum::x)
      .def_rw("value", &Minimum::value)
      .def_rw("iterations", &Minimum::iterations)
      .def_rw("converged", &Minimum::converged);
  nb::class_<NelderMeadOptions>(m, "NelderMeadOptions")
      .def(
          "__init__",
          [](NelderMeadOptions* self, double f_tol, double x_tol,
             int max_iterations) {
            new (self) NelderMeadOptions{f_tol, x_tol, max_iterations};
          },
          nb::arg("f_tol") = 1e-12, nb::arg("x_tol") = 1e-10,
          nb::arg("max_iterations") = 10'000)
      .def_rw("f_tol", &NelderMeadOptions::f_tol)
      .def_rw("x_tol", &NelderMeadOptions::x_tol)
      .def_rw("max_iterations", &NelderMeadOptions::max_iterations);
  m.def(
      "nelder_mead",
      [](const std::function<double(std::vector<double>)>& f,
         std::vector<double> x0, const std::vector<double>& step,
         const NelderMeadOptions* options) {
        if (step.size() != x0.size()) {
          throw nb::value_error("step must have the same size as x0");
        }
        const math::Objective objective = [&f](std::span<const double> x) {
          return f(std::vector<double>(x.begin(), x.end()));
        };
        return math::NelderMead(objective, x0, step,
                                options ? *options : NelderMeadOptions{});
      },
      nb::arg("f"), nb::arg("x0"), nb::arg("step"),
      nb::arg("options").none() = nb::none());
}

}  // namespace ofod::bindings
