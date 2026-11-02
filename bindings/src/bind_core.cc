// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>

#include "bindings.h"
#include "ofod/core/compounding.h"
#include "ofod/core/date.h"

namespace ofod::bindings {
namespace {

namespace nb = nanobind;

// Converts a Python datetime.date (or anything with year/month/day).
Date ToDate(const nb::handle& d) {
  return Date(nb::cast<int>(d.attr("year")),
              nb::cast<unsigned>(d.attr("month")),
              nb::cast<unsigned>(d.attr("day")));
}

}  // namespace

void BindCore(nb::module_& m) {
  nb::enum_<Compounding::Kind>(m, "CompoundingKind")
      .value("CONTINUOUS", Compounding::Kind::kContinuous)
      .value("PERIODIC", Compounding::Kind::kPeriodic)
      .value("SIMPLE", Compounding::Kind::kSimple);

  nb::class_<Compounding>(m, "Compounding")
      .def_static("continuous", &Compounding::Continuous)
      .def_static("periodic", &Compounding::Periodic, nb::arg("frequency"))
      .def_static("simple", &Compounding::Simple)
      .def_prop_ro("kind", &Compounding::kind)
      .def_prop_ro("frequency", &Compounding::frequency)
      .def(nb::self == nb::self)
      .def("__repr__", [](const Compounding& c) {
        return nb::str("Compounding(kind={}, frequency={})")
            .format(nb::cast(c.kind()), c.frequency());
      });

  m.def("growth_factor", &GrowthFactor, nb::arg("rate"), nb::arg("t"),
        nb::arg("c"));
  m.def("discount_factor", &DiscountFactor, nb::arg("rate"), nb::arg("t"),
        nb::arg("c"));
  m.def("implied_rate", &ImpliedRate, nb::arg("growth"), nb::arg("t"),
        nb::arg("c"));
  m.def("equivalent_rate", &EquivalentRate, nb::arg("rate"), nb::arg("from_c"),
        nb::arg("to_c"), nb::arg("t") = 1.0);

  nb::enum_<DayCount>(m, "DayCount")
      .value("ACTUAL_360", DayCount::kActual360)
      .value("ACTUAL_365_FIXED", DayCount::kActual365Fixed)
      .value("THIRTY_360", DayCount::kThirty360)
      .value("ACTUAL_ACTUAL_ISDA", DayCount::kActualActualIsda);

  m.def(
      "year_fraction",
      [](nb::handle start, nb::handle end, DayCount convention) {
        return YearFraction(ToDate(start), ToDate(end), convention);
      },
      nb::arg("start"), nb::arg("end"), nb::arg("convention"));
}

}  // namespace ofod::bindings
