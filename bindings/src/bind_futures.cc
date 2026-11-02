// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <string>
#include <vector>

#include "bindings.h"
#include "ofod/futures/margin.h"
#include "ofod/futures/rate_futures.h"
#include "ofod/futures/treasury.h"

namespace ofod::bindings {

namespace nb = nanobind;

void BindFutures(nb::module_& m) {
  using futures::CheapestToDeliver;
  using futures::DeliverableBond;
  using futures::MarginDay;
  using futures::MarginSpec;
  using instruments::Side;

  nb::class_<MarginSpec>(m, "MarginSpec")
      .def(
          "__init__",
          [](MarginSpec* self, double initial_margin, double maintenance_margin,
             double contract_size, int contracts, Side side) {
            new (self) MarginSpec{initial_margin, maintenance_margin,
                                  contract_size, contracts, side};
          },
          nb::arg("initial_margin") = 0.0, nb::arg("maintenance_margin") = 0.0,
          nb::arg("contract_size") = 1.0, nb::arg("contracts") = 1,
          nb::arg("side") = Side::kLong)
      .def_rw("initial_margin", &MarginSpec::initial_margin)
      .def_rw("maintenance_margin", &MarginSpec::maintenance_margin)
      .def_rw("contract_size", &MarginSpec::contract_size)
      .def_rw("contracts", &MarginSpec::contracts)
      .def_rw("side", &MarginSpec::side);

  nb::class_<MarginDay>(m, "MarginDay")
      .def(nb::init<double, double, double, double, double>(), nb::arg("price"),
           nb::arg("daily_gain"), nb::arg("cumulative_gain"),
           nb::arg("balance"), nb::arg("margin_call"))
      .def_rw("price", &MarginDay::price)
      .def_rw("daily_gain", &MarginDay::daily_gain)
      .def_rw("cumulative_gain", &MarginDay::cumulative_gain)
      .def_rw("balance", &MarginDay::balance)
      .def_rw("margin_call", &MarginDay::margin_call);
  m.def(
      "simulate_margin_account",
      [](const MarginSpec& spec, const std::vector<double>& prices) {
        return futures::SimulateMarginAccount(spec, prices);
      },
      nb::arg("spec"), nb::arg("prices"));

  m.def(
      "parse_thirty_seconds",
      [](const std::string& quote) {
        const auto q = futures::ParseThirtySeconds(quote);
        if (!q) throw nb::value_error("not a 32nds quote");
        return *q;
      },
      nb::arg("quote"));
  m.def(
      "accrued_interest",
      [](nb::handle last_coupon, nb::handle next_coupon, nb::handle settlement,
         double coupon_amount) {
        return futures::AccruedInterest(ToDate(last_coupon),
                                        ToDate(next_coupon), ToDate(settlement),
                                        coupon_amount);
      },
      nb::arg("last_coupon"), nb::arg("next_coupon"), nb::arg("settlement"),
      nb::arg("coupon_amount"));
  m.def("cash_price", &futures::CashPrice, nb::arg("quoted_price"),
        nb::arg("accrued_interest_"));
  m.def("conversion_factor", &futures::ConversionFactor, nb::arg("coupon_rate"),
        nb::arg("years_to_maturity"));

  nb::class_<DeliverableBond>(m, "DeliverableBond")
      .def(nb::init<double, double>(), nb::arg("quoted_price"),
           nb::arg("conversion_factor"))
      .def_rw("quoted_price", &DeliverableBond::quoted_price)
      .def_rw("conversion_factor", &DeliverableBond::conversion_factor);
  nb::class_<CheapestToDeliver>(m, "CheapestToDeliver")
      .def(nb::init<int, double>(), nb::arg("index"), nb::arg("cost"))
      .def_rw("index", &CheapestToDeliver::index)
      .def_rw("cost", &CheapestToDeliver::cost);
  m.def(
      "find_cheapest_to_deliver",
      [](double futures_price, const std::vector<DeliverableBond>& bonds) {
        const auto ctd = futures::FindCheapestToDeliver(futures_price, bonds);
        if (!ctd) throw nb::value_error("no deliverable bonds");
        return *ctd;
      },
      nb::arg("futures_price"), nb::arg("bonds"));
  m.def("treasury_futures_quote", &futures::TreasuryFuturesQuote,
        nb::arg("cash_bond_price"), nb::arg("coupons_pv"), nb::arg("rate"),
        nb::arg("t"), nb::arg("accrued_at_delivery"),
        nb::arg("conversion_factor_"));

  m.def("futures_rate_from_quote", &futures::FuturesRateFromQuote,
        nb::arg("quote"));
  m.def("three_month_contract_value", &futures::ThreeMonthContractValue,
        nb::arg("quote"));
  m.def("convexity_adjustment", &futures::ConvexityAdjustment, nb::arg("sigma"),
        nb::arg("t1"), nb::arg("t2"));
  m.def("duration_hedge_contracts", &futures::DurationHedgeContracts,
        nb::arg("portfolio_value"), nb::arg("portfolio_duration"),
        nb::arg("futures_value"), nb::arg("futures_duration"));
}

}  // namespace ofod::bindings
