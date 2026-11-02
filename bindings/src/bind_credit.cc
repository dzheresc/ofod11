// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <vector>

#include "bindings.h"
#include "ofod/credit/tranche.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindCredit(nb::module_& m) {
  using credit::Tranche;

  nb::class_<Tranche>(m, "Tranche")
      .def(
          "__init__",
          [](Tranche* self, double attachment, double detachment) {
            new (self) Tranche{attachment, detachment};
          },
          nb::arg("attachment") = 0.0, nb::arg("detachment") = 1.0)
      .def_rw("attachment", &Tranche::attachment)
      .def_rw("detachment", &Tranche::detachment);
  m.def("tranche_loss", &credit::TrancheLoss, nb::arg("tranche"),
        nb::arg("pool_loss"));
  m.def(
      "waterfall_losses",
      [](const std::vector<Tranche>& tranches, double pool_loss) {
        return credit::WaterfallLosses(tranches, pool_loss);
      },
      nb::arg("tranches"), nb::arg("pool_loss"));
}

}  // namespace ofod::bindings
