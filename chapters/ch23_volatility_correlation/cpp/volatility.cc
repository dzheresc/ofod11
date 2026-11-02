// Copyright 2026 Denis Zhereshchin
//
// Chapter 23: EWMA and GARCH(1,1) updates, a maximum likelihood GARCH fit
// with Ljung-Box diagnostics, the GARCH volatility term structure, EWMA
// correlation and a positive semi-definite check.

#include "ofod/stats/volatility.h"

#include <cmath>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ofod/math/linalg.h"

namespace {

namespace s = ofod::stats;

std::vector<double> ReadColumn(const std::string& path) {
  std::vector<double> x;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line[0] != '#') x.push_back(std::stod(line));
  }
  return x;
}

double LjungBoxOf(const std::vector<double>& u, const std::vector<double>& v) {
  std::vector<double> z;
  for (std::size_t i = 1; i < u.size(); ++i) z.push_back(u[i] * u[i] / v[i]);
  return s::LjungBox(z, 15).value_or(0.0);
}

}  // namespace

int main() {
  std::cout << std::format(
      "EWMA, lambda 0.90: vol 1%, return 2% -> {:.2f}%\n",
      100.0 * std::sqrt(s::EwmaVarianceUpdate(1e-4, 0.02, 0.90)));
  const s::Garch11 hull{.omega = 0.000002, .alpha = 0.13, .beta = 0.86};
  std::cout << std::format(
      "GARCH(0.000002, 0.13, 0.86): V_L = {:.4f} ({:.2f}% a day); vol 1.6%, "
      "return 1% -> {:.2f}%\n",
      hull.LongRunVariance(), 100.0 * std::sqrt(hull.LongRunVariance()),
      100.0 * std::sqrt(hull.Update(0.016 * 0.016, 0.01)));
  const double cov =
      s::EwmaCovarianceUpdate(0.6 * 0.01 * 0.02, 0.005, 0.025, 0.95);
  const double vx = s::EwmaVarianceUpdate(1e-4, 0.005, 0.95);
  const double vy = s::EwmaVarianceUpdate(4e-4, 0.025, 0.95);
  std::cout << std::format(
      "EWMA correlation, lambda 0.95: 0.6 -> {:.4f} (vols {:.3f}%, {:.3f}%)\n",
      cov / std::sqrt(vx * vy), 100.0 * std::sqrt(vx), 100.0 * std::sqrt(vy));

  const std::vector<double> u =
      ReadColumn(std::string(OFOD_DATA_DIR) + "/garch_returns.csv");
  const auto fit = s::FitGarch11(u);
  if (!fit) return 1;
  std::cout << std::format(
      "GARCH(1,1) fit to {} simulated returns (truth 2e-6, 0.13, 0.86):\n"
      "  omega {:.3e}, alpha {:.4f}, beta {:.4f}, long-run vol {:.3f}% a "
      "day\n",
      u.size(), fit->omega, fit->alpha, fit->beta,
      100.0 * std::sqrt(fit->LongRunVariance()));
  std::cout << std::format(
      "  log likelihood: GARCH {:.2f}, EWMA(0.94) {:.2f}\n",
      fit->LogLikelihood(u), s::EwmaLogLikelihood(u, 0.94));
  const std::vector<double> ones(u.size(), 1.0);
  std::cout << std::format(
      "  Ljung-Box (15 lags, critical 25.0): u^2 {:.1f}, u^2 / sigma^2 "
      "{:.1f}\n",
      LjungBoxOf(u, ones), LjungBoxOf(u, fit->Variances(u)));

  const double today = fit->Update(fit->Variances(u).back(), u.back());
  std::cout << std::format(
      "Volatility term structure from today's {:.2f}% a day (annualised)\n",
      100.0 * std::sqrt(today));
  for (double days : {10.0, 30.0, 50.0, 100.0, 500.0}) {
    std::cout << std::format("  {:>3.0f} days: {:.2f}%\n", days,
                             100.0 * fit->TermStructureVolatility(today, days));
  }

  const ofod::math::Matrix m = {
      {1.0, 0.0, 0.9}, {0.0, 1.0, 0.9}, {0.9, 0.9, 1.0}};
  const auto eigen = ofod::math::SymmetricEigen(m);
  std::cout << std::format(
      "[[1,0,0.9],[0,1,0.9],[0.9,0.9,1]] positive semi-definite: {} "
      "(smallest eigenvalue {:.4f})\n",
      s::IsPositiveSemidefinite(m) ? "yes" : "no",
      eigen ? eigen->values.back() : 0.0);
  return 0;
}
