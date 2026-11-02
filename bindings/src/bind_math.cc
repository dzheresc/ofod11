// Copyright 2026 Denis Zhereshchin
//
// C++ names follow the Google C++ style; Python names follow the Google Python
// style, so CamelCase functions are exposed as snake_case.

#include <nanobind/nanobind.h>

#include "ofod/math/normal.h"

namespace ofod::bindings {

void BindMath(nanobind::module_& m) {
  namespace nb = nanobind;
  nb::module_ math = m.def_submodule("math", "ofod::math");
  math.def("norm_pdf", &ofod::math::NormPdf, nb::arg("x"));
  math.def("norm_cdf", &ofod::math::NormCdf, nb::arg("x"));
  math.def("norm_cdf_poly", &ofod::math::NormCdfPoly, nb::arg("x"));
}

}  // namespace ofod::bindings
