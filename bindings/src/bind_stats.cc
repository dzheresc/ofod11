// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <optional>
#include <vector>

#include "bindings.h"
#include "ofod/stats/descriptive.h"
#include "ofod/stats/volatility.h"

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

  // Chapter 23: volatilities and correlations.
  m.def("ewma_variance_update", &stats::EwmaVarianceUpdate, nb::arg("variance"),
        nb::arg("ret"), nb::arg("lambda_"));
  m.def("ewma_covariance_update", &stats::EwmaCovarianceUpdate,
        nb::arg("covariance"), nb::arg("ret_x"), nb::arg("ret_y"),
        nb::arg("lambda_"));
  m.def(
      "ewma_variances",
      [](const std::vector<double>& returns, double lambda, double initial) {
        return stats::EwmaVariances(returns, lambda, initial);
      },
      nb::arg("returns"), nb::arg("lambda_"), nb::arg("initial"));

  using stats::Garch11;
  nb::class_<Garch11>(m, "Garch11")
      .def(
          "__init__",
          [](Garch11* self, double omega, double alpha, double beta) {
            new (self) Garch11{omega, alpha, beta};
          },
          nb::arg("omega") = 0.0, nb::arg("alpha") = 0.0, nb::arg("beta") = 0.0)
      .def_rw("omega", &Garch11::omega)
      .def_rw("alpha", &Garch11::alpha)
      .def_rw("beta", &Garch11::beta)
      .def("long_run_variance", &Garch11::LongRunVariance)
      .def("update", &Garch11::Update, nb::arg("variance"), nb::arg("ret"))
      .def("expected_variance", &Garch11::ExpectedVariance, nb::arg("variance"),
           nb::arg("days"))
      .def("term_structure_volatility", &Garch11::TermStructureVolatility,
           nb::arg("variance"), nb::arg("days"),
           nb::arg("days_per_year") = 252.0)
      .def(
          "variances",
          [](const Garch11& g, const std::vector<double>& returns) {
            return g.Variances(returns);
          },
          nb::arg("returns"))
      .def(
          "log_likelihood",
          [](const Garch11& g, const std::vector<double>& returns) {
            return g.LogLikelihood(returns);
          },
          nb::arg("returns"));
  m.def(
      "fit_garch11",
      [](const std::vector<double>& returns) {
        const auto g = stats::FitGarch11(returns);
        if (!g) {
          throw nb::value_error(
              "need >= 10 returns with non-zero variance and a converged fit");
        }
        return *g;
      },
      nb::arg("returns"));
  m.def(
      "ewma_log_likelihood",
      [](const std::vector<double>& returns, double lambda) {
        return stats::EwmaLogLikelihood(returns, lambda);
      },
      nb::arg("returns"), nb::arg("lambda_"));
  m.def(
      "autocorrelation",
      [](const std::vector<double>& x, int lag) {
        return OrThrow(stats::Autocorrelation(x, lag),
                       "need 0 < lag < len(x) and a non-constant series");
      },
      nb::arg("x"), nb::arg("lag"));
  m.def(
      "ljung_box",
      [](const std::vector<double>& x, int lags) {
        return OrThrow(stats::LjungBox(x, lags), "need 0 < lags < len(x)");
      },
      nb::arg("x"), nb::arg("lags"));
  m.def(
      "is_positive_semidefinite",
      [](const math::Matrix& a, double tolerance) {
        return stats::IsPositiveSemidefinite(a, tolerance);
      },
      nb::arg("a"), nb::arg("tolerance") = 1e-12);
}

}  // namespace ofod::bindings
