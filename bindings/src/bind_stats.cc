// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <optional>
#include <vector>

#include "bindings.h"
#include "ofod/stats/descriptive.h"

namespace ofod::bindings {

namespace nb = nanobind;

namespace {

double OrThrow(std::optional<double> v, const char* message) {
  if (!v) throw nb::value_error(message);
  return *v;
}

}  // namespace

void BindStats(nb::module_& m) {
  using stats::Regression;
  using Vec = const std::vector<double>&;

  nb::class_<Regression>(m, "Regression")
      .def(nb::init<double, double, double>(), nb::arg("intercept"),
           nb::arg("slope"), nb::arg("r_squared"))
      .def_rw("intercept", &Regression::intercept)
      .def_rw("slope", &Regression::slope)
      .def_rw("r_squared", &Regression::r_squared);

  m.def(
      "mean",
      [](Vec x) { return OrThrow(stats::Mean(x), "mean of an empty sample"); },
      nb::arg("x"));
  m.def(
      "variance",
      [](Vec x) {
        return OrThrow(stats::Variance(x), "variance needs >= 2 points");
      },
      nb::arg("x"));
  m.def(
      "std_dev",
      [](Vec x) {
        return OrThrow(stats::StdDev(x), "std_dev needs >= 2 points");
      },
      nb::arg("x"));
  m.def(
      "covariance",
      [](Vec x, Vec y) {
        return OrThrow(stats::Covariance(x, y),
                       "covariance needs two equal samples of size >= 2");
      },
      nb::arg("x"), nb::arg("y"));
  m.def(
      "correlation",
      [](Vec x, Vec y) {
        return OrThrow(stats::Correlation(x, y),
                       "correlation needs two non-constant equal samples");
      },
      nb::arg("x"), nb::arg("y"));
  m.def(
      "linear_regression",
      [](Vec x, Vec y) {
        const auto fit = stats::LinearRegression(x, y);
        if (!fit) {
          throw nb::value_error(
              "regression needs equal samples and a non-constant regressor");
        }
        return *fit;
      },
      nb::arg("x"), nb::arg("y"));

  m.def(
      "log_returns", [](Vec prices) { return stats::LogReturns(prices); },
      nb::arg("prices"));
  m.def(
      "historical_volatility",
      [](Vec prices, double periods_per_year) {
        return OrThrow(stats::HistoricalVolatility(prices, periods_per_year),
                       "need at least three prices");
      },
      nb::arg("prices"), nb::arg("periods_per_year"));
}

}  // namespace ofod::bindings
