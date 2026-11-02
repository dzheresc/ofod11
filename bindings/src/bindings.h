// Copyright 2026 Denis Zhereshchin
//
// Registration functions, one per ofod module. C++ names follow the Google
// C++ style; they are exposed under the Python names used by the pure-Python
// `ofod` package (snake_case functions, UPPER_CASE enum members).
//
// The bindings are the one place that throws: nanobind turns
// nanobind::value_error into Python's ValueError where the C++ API returns
// std::nullopt.

#ifndef OFOD_BINDINGS_BINDINGS_H_
#define OFOD_BINDINGS_BINDINGS_H_

#include <nanobind/nanobind.h>

#include "ofod/core/date.h"

namespace ofod::bindings {

// Converts a Python datetime.date (or anything with year/month/day).
Date ToDate(const nanobind::handle& d);

void BindCore(nanobind::module_& m);
void BindMath(nanobind::module_& m);
void BindInstruments(nanobind::module_& m);
void BindCurves(nanobind::module_& m);
void BindPricers(nanobind::module_& m);
void BindStats(nanobind::module_& m);
void BindRisk(nanobind::module_& m);
void BindFutures(nanobind::module_& m);
void BindCredit(nanobind::module_& m);
void BindStrategies(nanobind::module_& m);

}  // namespace ofod::bindings

#endif  // OFOD_BINDINGS_BINDINGS_H_
