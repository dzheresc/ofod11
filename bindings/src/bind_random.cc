// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <vector>

#include "bindings.h"
#include "ofod/random/normal_generator.h"

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
}

}  // namespace ofod::bindings
