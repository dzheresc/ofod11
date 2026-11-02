// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/vector.h>

#include <functional>
#include <utility>
#include <vector>

#include "bindings.h"
#include "ofod/instruments/bond.h"
#include "ofod/instruments/cash_flow.h"
#include "ofod/instruments/fra.h"
#include "ofod/instruments/option.h"
#include "ofod/instruments/payoff.h"
#include "ofod/instruments/portfolio.h"
#include "ofod/instruments/swap.h"

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

  using instruments::CurrencySwap;
  using instruments::InterestRateSwap;
  nb::class_<InterestRateSwap>(m, "InterestRateSwap")
      .def(
          "__init__",
          [](InterestRateSwap* self, double notional, double fixed_rate,
             int frequency, std::vector<double> payment_times,
             double next_floating_rate) {
            new (self)
                InterestRateSwap{notional, fixed_rate, frequency,
                                 std::move(payment_times), next_floating_rate};
          },
          nb::arg("notional") = 0.0, nb::arg("fixed_rate") = 0.0,
          nb::arg("frequency") = 2,
          nb::arg("payment_times") = std::vector<double>{},
          nb::arg("next_floating_rate") = 0.0)
      .def_rw("notional", &InterestRateSwap::notional)
      .def_rw("fixed_rate", &InterestRateSwap::fixed_rate)
      .def_rw("frequency", &InterestRateSwap::frequency)
      .def_rw("payment_times", &InterestRateSwap::payment_times)
      .def_rw("next_floating_rate", &InterestRateSwap::next_floating_rate);

  nb::class_<CurrencySwap>(m, "CurrencySwap")
      .def(
          "__init__",
          [](CurrencySwap* self, double domestic_notional, double domestic_rate,
             double foreign_notional, double foreign_rate, int frequency,
             std::vector<double> payment_times) {
            new (self) CurrencySwap{
                domestic_notional, domestic_rate, foreign_notional,
                foreign_rate,      frequency,     std::move(payment_times)};
          },
          nb::arg("domestic_notional") = 0.0, nb::arg("domestic_rate") = 0.0,
          nb::arg("foreign_notional") = 0.0, nb::arg("foreign_rate") = 0.0,
          nb::arg("frequency") = 1,
          nb::arg("payment_times") = std::vector<double>{})
      .def_rw("domestic_notional", &CurrencySwap::domestic_notional)
      .def_rw("domestic_rate", &CurrencySwap::domestic_rate)
      .def_rw("foreign_notional", &CurrencySwap::foreign_notional)
      .def_rw("foreign_rate", &CurrencySwap::foreign_rate)
      .def_rw("frequency", &CurrencySwap::frequency)
      .def_rw("payment_times", &CurrencySwap::payment_times);

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

  using instruments::ContractTerms;
  using instruments::Exercise;
  using instruments::OptionType;
  using instruments::VanillaOption;
  nb::enum_<OptionType>(m, "OptionType")
      .value("CALL", OptionType::kCall)
      .value("PUT", OptionType::kPut);
  nb::enum_<Exercise>(m, "Exercise")
      .value("EUROPEAN", Exercise::kEuropean)
      .value("AMERICAN", Exercise::kAmerican);
  nb::class_<VanillaOption>(m, "VanillaOption")
      .def(
          "__init__",
          [](VanillaOption* self, OptionType option_type, Exercise exercise,
             double strike, double maturity) {
            new (self) VanillaOption{option_type, exercise, strike, maturity};
          },
          nb::arg("option_type") = OptionType::kCall,
          nb::arg("exercise") = Exercise::kEuropean, nb::arg("strike") = 0.0,
          nb::arg("maturity") = 0.0)
      .def_rw("option_type", &VanillaOption::option_type)
      .def_rw("exercise", &VanillaOption::exercise)
      .def_rw("strike", &VanillaOption::strike)
      .def_rw("maturity", &VanillaOption::maturity);
  m.def("intrinsic_value", &instruments::IntrinsicValue, nb::arg("option"),
        nb::arg("spot"));

  nb::class_<ContractTerms>(m, "ContractTerms")
      .def(
          "__init__",
          [](ContractTerms* self, double strike, double shares) {
            new (self) ContractTerms{strike, shares};
          },
          nb::arg("strike") = 0.0, nb::arg("shares") = 100.0)
      .def_rw("strike", &ContractTerms::strike)
      .def_rw("shares", &ContractTerms::shares);
  m.def("adjust_for_split", &instruments::AdjustForSplit, nb::arg("terms"),
        nb::arg("n"), nb::arg("m"));
  m.def("adjust_for_stock_dividend", &instruments::AdjustForStockDividend,
        nb::arg("terms"), nb::arg("fraction"));
  m.def("naked_option_margin", &instruments::NakedOptionMargin,
        nb::arg("option_type"), nb::arg("premium"), nb::arg("spot"),
        nb::arg("strike"));

  nb::class_<instruments::StockPayoff>(m, "StockPayoff")
      .def(nb::init<>())
      .def("__call__", &instruments::StockPayoff::operator(), nb::arg("spot"));
  nb::class_<instruments::CashPayoff>(m, "CashPayoff")
      .def(nb::init<double>(), nb::arg("amount"))
      .def_rw("amount", &instruments::CashPayoff::amount)
      .def("__call__", &instruments::CashPayoff::operator(), nb::arg("spot"));

  using instruments::Leg;
  using instruments::Portfolio;
  nb::class_<Leg>(m, "Leg")
      .def(
          "__init__",
          [](Leg* self, double quantity, std::function<double(double)> payoff,
             double premium) {
            new (self) Leg{quantity, std::move(payoff), premium};
          },
          nb::arg("quantity"), nb::arg("payoff"), nb::arg("premium") = 0.0)
      .def_rw("quantity", &Leg::quantity)
      .def_rw("payoff", &Leg::payoff)
      .def_rw("premium", &Leg::premium);
  nb::class_<Portfolio>(m, "Portfolio")
      .def(nb::init<>())
      .def("add", &Portfolio::Add, nb::arg("quantity"), nb::arg("payoff"),
           nb::arg("premium") = 0.0, nb::rv_policy::reference_internal)
      .def("payoff_at", &Portfolio::PayoffAt, nb::arg("spot"))
      .def("cost", &Portfolio::Cost)
      .def("profit_at", &Portfolio::ProfitAt, nb::arg("spot"))
      .def_prop_ro("legs", [](const Portfolio& p) {
        return std::vector<Leg>(p.legs().begin(), p.legs().end());
      });
}

}  // namespace ofod::bindings
