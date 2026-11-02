// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "bindings.h"
#include "ofod/random/normal_generator.h"
#include "ofod/random/sobol.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindRandom(nb::module_& m) {
  using random::Matrix;
  using random::NormalGenerator;

  nb::class_<NormalGenerator>(m, "NormalGenerator")
      .def(nb::init<std::uint64_t>(), nb::arg("seed"))
      .def("next", &NormalGenerator::Next)
      .def("sample", &NormalGenerator::Sample, nb::arg("n"));
  m.def(
      "cholesky",
      [](const Matrix& a) {
        auto l = random::Cholesky(a);
        if (!l) {
          throw nb::value_error(
              "matrix must be square, symmetric and positive definite");
        }
        return *std::move(l);
      },
      nb::arg("a"));
  m.def(
      "correlate",
      [](const Matrix& lower, const std::vector<double>& z) {
        return random::Correlate(lower, z);
      },
      nb::arg("lower"), nb::arg("z"));

  nb::class_<random::Sobol>(m, "Sobol")
      .def(
          "__init__",
          [](random::Sobol* self, int dimensions) {
            auto sobol = random::Sobol::Create(dimensions);
            if (!sobol) throw nb::value_error("dimensions must be in [1, 8]");
            new (self) random::Sobol(*std::move(sobol));
          },
          nb::arg("dimensions"))
      .def_ro_static("MAX_DIMENSIONS", &random::Sobol::kMaxDimensions)
      .def_prop_ro("dimensions", &random::Sobol::dimensions)
      .def("next", &random::Sobol::Next);
}

}  // namespace ofod::bindings
