// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/vector.h>

#include <functional>

#include "bindings.h"
#include "ofod/instruments/bond.h"
#include "ofod/instruments/cash_flow.h"
#include "ofod/instruments/fra.h"
#include "ofod/instruments/payoff.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindInstruments(nb::module_& m) {
  using instruments::Bond;
  using instruments::CashFlow;
  using instruments::Fra;
  using instruments::Side;

  nb::enum_<Side>(m, "Side")
      .value("LONG", Side::kLong)
      .value("SHORT", Side::kShort);
  m.def("sign", &instruments::Sign, nb::arg("side"));

  nb::class_<CashFlow>(m, "CashFlow")
      .def(nb::init<double, double>(), nb::arg("time"), nb::arg("amount"))
      .def_rw("time", &CashFlow::time)
      .def_rw("amount", &CashFlow::amount);

  nb::class_<Bond>(m, "Bond")
      .def(
          "__init__",
          [](Bond* self, double face, double coupon_rate, double maturity,
             int frequency) {
            new (self) Bond{face, coupon_rate, maturity, frequency};
          },
          nb::arg("face") = 100.0, nb::arg("coupon_rate") = 0.0,
          nb::arg("maturity") = 0.0, nb::arg("frequency") = 2)
      .def_rw("face", &Bond::face)
      .def_rw("coupon_rate", &Bond::coupon_rate)
      .def_rw("maturity", &Bond::maturity)
      .def_rw("frequency", &Bond::frequency);
  m.def("cash_flows", &instruments::CashFlows, nb::arg("bond"));

  nb::class_<Fra>(m, "Fra")
      .def(
          "__init__",
          [](Fra* self, double notional, double fixed_rate, double start,
             double end) { new (self) Fra{notional, fixed_rate, start, end}; },
          nb::arg("notional") = 0.0, nb::arg("fixed_rate") = 0.0,
          nb::arg("start") = 0.0, nb::arg("end") = 0.0)
      .def_rw("notional", &Fra::notional)
      .def_rw("fixed_rate", &Fra::fixed_rate)
      .def_rw("start", &Fra::start)
      .def_rw("end", &Fra::end);

  nb::class_<instruments::ForwardPayoff>(m, "ForwardPayoff")
      .def(nb::init<double>(), nb::arg("delivery_price"))
      .def_rw("delivery_price", &instruments::ForwardPayoff::delivery_price)
      .def("__call__", &instruments::ForwardPayoff::operator(),
           nb::arg("spot"));
  nb::class_<instruments::CallPayoff>(m, "CallPayoff")
      .def(nb::init<double>(), nb::arg("strike"))
      .def_rw("strike", &instruments::CallPayoff::strike)
      .def("__call__", &instruments::CallPayoff::operator(), nb::arg("spot"));
  nb::class_<instruments::PutPayoff>(m, "PutPayoff")
      .def(nb::init<double>(), nb::arg("strike"))
      .def_rw("strike", &instruments::PutPayoff::strike)
      .def("__call__", &instruments::PutPayoff::operator(), nb::arg("spot"));

  // Python payoffs are any callable, so the binding type-erases them.
  using AnyPayoff = std::function<double(double)>;
  using AnyPosition = instruments::Position<AnyPayoff>;
  nb::class_<AnyPosition>(m, "Position")
      .def(
          "__init__",
          [](AnyPosition* self, AnyPayoff payoff, Side side, double quantity,
             double premium) {
            new (self) AnyPosition{std::move(payoff), side, quantity, premium};
          },
          nb::arg("payoff"), nb::arg("side") = Side::kLong,
          nb::arg("quantity") = 1.0, nb::arg("premium") = 0.0)
      .def_rw("side", &AnyPosition::side)
      .def_rw("quantity", &AnyPosition::quantity)
      .def_rw("premium", &AnyPosition::premium)
      .def("payoff_at", &AnyPosition::PayoffAt, nb::arg("spot"))
      .def("profit_at", &AnyPosition::ProfitAt, nb::arg("spot"));
}

}  // namespace ofod::bindings
