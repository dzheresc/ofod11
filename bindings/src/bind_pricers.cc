// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <vector>

#include "bindings.h"
#include "ofod/pricers/bond_pricer.h"
#include "ofod/pricers/forward_pricer.h"
#include "ofod/pricers/fra_pricer.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindPricers(nb::module_& m) {
  using instruments::Bond;
  const Compounding continuous = Compounding::Continuous();

  m.def("bond_price", &pricers::BondPrice, nb::arg("bond"), nb::arg("curve"));
  m.def("bond_price_from_yield", &pricers::BondPriceFromYield, nb::arg("bond"),
        nb::arg("yield_"), nb::arg("c") = continuous);
  m.def(
      "bond_yield",
      [](const Bond& bond, double price, Compounding c) {
        const auto y = pricers::BondYield(bond, price, c);
        if (!y) throw nb::value_error("no yield reprices the bond");
        return *y;
      },
      nb::arg("bond"), nb::arg("price"), nb::arg("c") = continuous);
  m.def("macaulay_duration", &pricers::MacaulayDuration, nb::arg("bond"),
        nb::arg("yield_"), nb::arg("c") = continuous);
  m.def("modified_duration", &pricers::ModifiedDuration, nb::arg("bond"),
        nb::arg("yield_"), nb::arg("c"));
  m.def("convexity", &pricers::Convexity, nb::arg("bond"), nb::arg("yield_"));
  m.def("par_yield", &pricers::ParYield, nb::arg("maturity"),
        nb::arg("frequency"), nb::arg("curve"));

  m.def("simple_forward_rate", &pricers::SimpleForwardRate, nb::arg("curve"),
        nb::arg("t1"), nb::arg("t2"));
  m.def("fra_value", &pricers::FraValue, nb::arg("fra"), nb::arg("curve"),
        nb::arg("side"));

  m.def("forward_price", &pricers::ForwardPrice, nb::arg("spot"),
        nb::arg("rate"), nb::arg("t"));
  m.def("forward_price_with_income", &pricers::ForwardPriceWithIncome,
        nb::arg("spot"), nb::arg("income_pv"), nb::arg("rate"), nb::arg("t"));
  m.def("forward_price_with_yield", &pricers::ForwardPriceWithYield,
        nb::arg("spot"), nb::arg("rate"), nb::arg("yield_"), nb::arg("t"));
  m.def("forward_price_cost_of_carry", &pricers::ForwardPriceCostOfCarry,
        nb::arg("spot"), nb::arg("rate"), nb::arg("storage_yield"),
        nb::arg("convenience_yield"), nb::arg("t"));
  m.def(
      "present_value",
      [](const std::vector<instruments::CashFlow>& flows,
         const curves::ZeroCurve& curve) {
        return pricers::PresentValue(flows, curve);
      },
      nb::arg("flows"), nb::arg("curve"));
  m.def("forward_contract_value", &pricers::ForwardContractValue,
        nb::arg("forward_price_"), nb::arg("delivery_price"), nb::arg("rate"),
        nb::arg("t"), nb::arg("side"));

  nb::enum_<pricers::ForwardArbitrage>(m, "ForwardArbitrage")
      .value("NONE", pricers::ForwardArbitrage::kNone)
      .value("CASH_AND_CARRY", pricers::ForwardArbitrage::kCashAndCarry)
      .value("REVERSE_CASH_AND_CARRY",
             pricers::ForwardArbitrage::kReverseCashAndCarry);
  m.def("detect_forward_arbitrage", &pricers::DetectForwardArbitrage,
        nb::arg("quoted_forward"), nb::arg("theoretical"),
        nb::arg("tolerance") = 1e-9);
}

}  // namespace ofod::bindings
