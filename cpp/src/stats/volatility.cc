// Copyright 2026 Denis Zhereshchin

#include "ofod/stats/volatility.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "ofod/math/linalg.h"
#include "ofod/math/optimize.h"

namespace ofod::stats {

double EwmaVarianceUpdate(double variance, double ret, double lambda) {
  return lambda * variance + (1.0 - lambda) * ret * ret;
}

double EwmaCovarianceUpdate(double covariance, double ret_x, double ret_y,
                            double lambda) {
  return lambda * covariance + (1.0 - lambda) * ret_x * ret_y;
}

std::vector<double> EwmaVariances(std::span<const double> returns,
                                  double lambda, double initial) {
  std::vector<double> v = {initial};
  v.reserve(returns.size() + 1);
  for (double u : returns) v.push_back(EwmaVarianceUpdate(v.back(), u, lambda));
  return v;
}

double Garch11::LongRunVariance() const { return omega / (1.0 - alpha - beta); }

double Garch11::Update(double variance, double ret) const {
  return omega + alpha * ret * ret + beta * variance;
}

double Garch11::ExpectedVariance(double variance, double days) const {
  const double vl = LongRunVariance();
  return vl + std::pow(alpha + beta, days) * (variance - vl);
}

double Garch11::TermStructureVolatility(double variance, double days,
                                        double days_per_year) const {
  const double vl = LongRunVariance();
  const double a = std::log(1.0 / (alpha + beta));
  const double factor = (1.0 - std::exp(-a * days)) / (a * days);
  return std::sqrt(days_per_year * (vl + factor * (variance - vl)));
}

std::vector<double> Garch11::Variances(std::span<const double> returns) const {
  std::vector<double> v(returns.size());
  if (returns.empty()) return v;
  v[0] = returns[0] * returns[0];
  if (returns.size() > 1) v[1] = v[0];
  for (std::size_t i = 2; i < returns.size(); ++i) {
    v[i] = Update(v[i - 1], returns[i - 1]);
  }
  return v;
}

namespace {

// sum_{i >= 1} (-ln v_i - u_i^2 / v_i), or -inf if any v_i <= 0.
double GaussianLogLikelihood(std::span<const double> returns,
                             std::span<const double> v) {
  double ll = 0.0;
  for (std::size_t i = 1; i < returns.size(); ++i) {
    if (!(v[i] > 0.0)) return -std::numeric_limits<double>::infinity();
    ll += -std::log(v[i]) - returns[i] * returns[i] / v[i];
  }
  return ll;
}

}  // namespace

double Garch11::LogLikelihood(std::span<const double> returns) const {
  const std::vector<double> v = Variances(returns);
  return GaussianLogLikelihood(returns, v);
}

std::optional<Garch11> FitGarch11(std::span<const double> returns) {
  if (returns.size() < 10) return std::nullopt;
  double sum_sq = 0.0;
  for (double u : returns) sum_sq += u * u;
  const double s2 = sum_sq / static_cast<double>(returns.size());
  if (!(s2 > 0.0)) return std::nullopt;
  const auto objective = [&](std::span<const double> x) {
    const Garch11 g{.omega = x[0] * s2, .alpha = x[1], .beta = x[2]};
    if (x[0] <= 0.0 || g.alpha < 0.0 || g.beta < 0.0 ||
        g.alpha + g.beta >= 1.0) {
      return std::numeric_limits<double>::max();
    }
    const double ll = g.LogLikelihood(returns);
    return std::isfinite(ll) ? -ll : std::numeric_limits<double>::max();
  };
  constexpr std::array<double, 3> kStep = {0.02, 0.05, 0.05};
  const math::Minimum m = math::NelderMead(objective, {0.05, 0.10, 0.85}, kStep,
                                           {.f_tol = 1e-9, .x_tol = 1e-9});
  if (!m.converged) return std::nullopt;
  return Garch11{.omega = m.x[0] * s2, .alpha = m.x[1], .beta = m.x[2]};
}

double EwmaLogLikelihood(std::span<const double> returns, double lambda) {
  if (returns.size() < 2) return 0.0;
  std::vector<double> v(returns.size());
  v[0] = returns[0] * returns[0];
  v[1] = v[0];
  for (std::size_t i = 2; i < returns.size(); ++i) {
    v[i] = EwmaVarianceUpdate(v[i - 1], returns[i - 1], lambda);
  }
  return GaussianLogLikelihood(returns, v);
}

std::optional<double> Autocorrelation(std::span<const double> x, int lag) {
  const auto n = static_cast<int>(x.size());
  if (lag <= 0 || lag >= n) return std::nullopt;
  double mean = 0.0;
  for (double v : x) mean += v;
  mean /= n;
  double num = 0.0;
  double den = 0.0;
  for (int i = 0; i < n; ++i) {
    const double d = x[static_cast<std::size_t>(i)] - mean;
    den += d * d;
    if (i >= lag) num += d * (x[static_cast<std::size_t>(i - lag)] - mean);
  }
  if (den == 0.0) return std::nullopt;
  return num / den;
}

std::optional<double> LjungBox(std::span<const double> x, int lags) {
  const auto n = static_cast<double>(x.size());
  if (lags <= 0 || lags >= static_cast<int>(x.size())) return std::nullopt;
  double q = 0.0;
  for (int k = 1; k <= lags; ++k) {
    const auto rho = Autocorrelation(x, k);
    if (!rho) return std::nullopt;
    q += *rho * *rho / (n - k);
  }
  return n * (n + 2.0) * q;
}

bool IsPositiveSemidefinite(const math::Matrix& a, double tolerance) {
  const auto eigen = math::SymmetricEigen(a);
  return eigen && eigen->values.back() >= -tolerance;
}

}  // namespace ofod::stats
