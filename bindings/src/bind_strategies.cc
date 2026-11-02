// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/vector.h>

#include <vector>

#include "bindings.h"
#include "ofod/strategies/strategies.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindStrategies(nb::module_& m) {
  namespace st = strategies;
  m.def("covered_call", &st::CoveredCall, nb::arg("spot"), nb::arg("strike"),
        nb::arg("call"));
  m.def("protective_put", &st::ProtectivePut, nb::arg("spot"),
        nb::arg("strike"), nb::arg("put"));
  m.def("bull_call_spread", &st::BullCallSpread, nb::arg("k1"), nb::arg("c1"),
        nb::arg("k2"), nb::arg("c2"));
  m.def("bull_put_spread", &st::BullPutSpread, nb::arg("k1"), nb::arg("p1"),
        nb::arg("k2"), nb::arg("p2"));
  m.def("bear_call_spread", &st::BearCallSpread, nb::arg("k1"), nb::arg("c1"),
        nb::arg("k2"), nb::arg("c2"));
  m.def("bear_put_spread", &st::BearPutSpread, nb::arg("k1"), nb::arg("p1"),
        nb::arg("k2"), nb::arg("p2"));
  m.def("box_spread", &st::BoxSpread, nb::arg("k1"), nb::arg("c1"),
        nb::arg("p1"), nb::arg("k2"), nb::arg("c2"), nb::arg("p2"));
  m.def("butterfly_call_spread", &st::ButterflyCallSpread, nb::arg("k1"),
        nb::arg("c1"), nb::arg("k2"), nb::arg("c2"), nb::arg("k3"),
        nb::arg("c3"));
  m.def("straddle", &st::Straddle, nb::arg("strike"), nb::arg("call"),
        nb::arg("put"));
  m.def("strip", &st::Strip, nb::arg("strike"), nb::arg("call"),
        nb::arg("put"));
  m.def("strap", &st::Strap, nb::arg("strike"), nb::arg("call"),
        nb::arg("put"));
  m.def("strangle", &st::Strangle, nb::arg("put_strike"), nb::arg("put"),
        nb::arg("call_strike"), nb::arg("call"));
  m.def("principal_protected_budget", &st::PrincipalProtectedBudget,
        nb::arg("principal"), nb::arg("rate"), nb::arg("t"));
  m.def(
      "replicate_payoff",
      [](double value_at_zero, const std::vector<double>& slopes,
         const std::vector<double>& kinks) {
        auto p = st::ReplicatePayoff(value_at_zero, slopes, kinks);
        if (!p) {
          throw nb::value_error(
              "need one more slope than kinks, with kinks strictly "
              "increasing");
        }
        return *std::move(p);
      },
      nb::arg("value_at_zero"), nb::arg("slopes"), nb::arg("kinks"));
}

}  // namespace ofod::bindings
