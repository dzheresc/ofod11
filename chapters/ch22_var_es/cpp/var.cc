// Copyright 2026 Denis Zhereshchin
//
// Chapter 22: the linear model, normal VaR and ES, historical simulation
// with and without volatility scaling, a Kupiec backtest, delta-gamma VaR
// vs Monte Carlo full revaluation, and PCA on rate changes.

#include "ofod/risk/var.h"

#include <cmath>
#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ofod/greeks/greeks.h"
#include "ofod/math/linalg.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/stats/volatility.h"

namespace {

namespace r = ofod::risk;
using ofod::math::Matrix;

// Reads comma-separated rows, skipping lines that start with '#'.
Matrix ReadCsv(const std::string& path) {
  Matrix rows;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::vector<double> row;
    std::stringstream cells(line);
    std::string cell;
    while (std::getline(cells, cell, ',')) row.push_back(std::stod(cell));
    rows.push_back(std::move(row));
  }
  return rows;
}

void LinearModel() {
  const double msft = r::ScaleToHorizon(r::NormalVar(200'000, 0.99), 10);
  const double att = r::ScaleToHorizon(r::NormalVar(50'000, 0.99), 10);
  const auto sd = r::PortfolioStdDev(std::vector<double>{10e6, 5e6},
                                     std::vector<double>{0.02, 0.01},
                                     {{1.0, 0.3}, {0.3, 1.0}});
  if (!sd) return;
  const double both = r::ScaleToHorizon(r::NormalVar(*sd, 0.99), 10);
  std::cout << "Linear model, 10-day 99% VaR\n";
  std::cout << std::format(
      "  Microsoft {:.0f}  AT&T {:.0f}  together {:.0f}  diversification "
      "benefit {:.0f}\n",
      msft, att, both, msft + att - both);
  std::cout << std::format(
      "Normal loss with sd 20: 99% VaR {:.2f}, 99% ES {:.2f}, 97.5% ES "
      "{:.2f}\n",
      r::NormalVar(20.0, 0.99), r::NormalExpectedShortfall(20.0, 0.99),
      r::NormalExpectedShortfall(20.0, 0.975));
}

void HistoricalAndBacktest(const std::vector<double>& u) {
  constexpr double kPosition = 10e6;
  std::vector<double> losses;
  losses.reserve(u.size());
  for (double x : u) losses.push_back(-kPosition * x);
  const auto fitted = ofod::stats::FitGarch11(u);
  if (!fitted) return;
  const ofod::stats::Garch11 fit = *fitted;
  const std::vector<double> v = fit.Variances(u);
  std::vector<double> vols;
  vols.reserve(v.size());
  for (double x : v) vols.push_back(std::sqrt(x));
  const double current = std::sqrt(fit.Update(v.back(), u.back()));
  const auto scaled = r::VolatilityAdjustedChanges(losses, vols, current);
  const auto plain_var = r::HistoricalVar(losses, 0.99);
  const auto plain_es = r::HistoricalExpectedShortfall(losses, 0.99);
  if (!scaled || !plain_var || !plain_es) return;
  const auto scaled_var = r::HistoricalVar(*scaled, 0.99);
  const auto scaled_es = r::HistoricalExpectedShortfall(*scaled, 0.99);
  if (!scaled_var || !scaled_es) return;
  std::cout << std::format(
      "Historical simulation, $10M on {} days of returns (1-day 99%)\n",
      u.size());
  std::cout << std::format("  plain:             VaR {:.0f}  ES {:.0f}\n",
                           *plain_var, *plain_es);
  std::cout << std::format(
      "  GARCH vol-scaled:  VaR {:.0f}  ES {:.0f}  (vol today {:.2f}%)\n",
      *scaled_var, *scaled_es, 100.0 * current);

  // Backtest a 99% EWMA (lambda = 0.94) VaR over the sample.
  const std::vector<double> ewma = ofod::stats::EwmaVariances(u, 0.94, v[1]);
  int exceptions = 0;
  for (std::size_t i = 1; i < u.size(); ++i) {
    const double var = r::NormalVar(kPosition * std::sqrt(ewma[i]), 0.99);
    if (losses[i] > var) ++exceptions;
  }
  const int days = static_cast<int>(u.size()) - 1;
  std::cout << std::format(
      "Backtest of 99% EWMA VaR: {} exceptions in {} days (expected {:.1f}), "
      "Kupiec {:.2f}, P(>= {}) = {:.3f}\n",
      exceptions, days, 0.01 * days, r::KupiecStatistic(days, exceptions, 0.01),
      exceptions, r::BinomialTailProbability(days, exceptions, 0.01));
}

void QuadraticVsMonteCarlo() {
  // Short 1000 calls, S = 50, K = 50, r = 5%, sigma = 25%, T = 0.25; the
  // stock's daily volatility is 1.6%.
  const ofod::greeks::OptionInputs in{.spot = 50.0,
                                      .rate = 0.05,
                                      .yield = 0.0,
                                      .volatility = 0.25,
                                      .maturity = 0.25};
  const auto g =
      ofod::greeks::BsmGreeks(ofod::instruments::OptionType::kCall, 50.0, in);
  const double sd_change = 50.0 * 0.016;
  const auto moments =
      r::DeltaGammaMoments(-1000.0 * g.delta, -1000.0 * g.gamma, sd_change);
  auto value = [](double spot) {
    return ofod::pricers::BsmPrice(ofod::instruments::OptionType::kCall, spot,
                                   50.0, 0.05, 0.0, 0.25, 0.25);
  };
  const double v0 = value(50.0);
  const r::PnlFunction pnl = [&](std::span<const double> dx) {
    return -1000.0 * (value(50.0 * (1.0 + dx[0])) - v0);
  };
  const auto mc = r::MonteCarloVar(pnl, {{0.016 * 0.016}}, 0.99, 100'000, 7);
  std::cout << "Short 1000 calls, 1-day 99% VaR\n";
  std::cout << std::format(
      "  delta-normal {:.1f}  Cornish-Fisher delta-gamma {:.1f}  Monte "
      "Carlo {:.1f}\n",
      r::NormalVar(1000.0 * g.delta * sd_change, 0.99),
      r::CornishFisherVar(moments, 0.99), mc ? mc->var : 0.0);
}

void Pca(const Matrix& changes) {
  const auto pca = r::PrincipalComponents(changes);
  if (!pca) return;
  double total = 0.0;
  for (double v : pca->variances) total += v;
  std::cout << "PCA of daily changes in 4 rates\n";
  for (std::size_t k = 0; k < pca->variances.size(); ++k) {
    std::cout << std::format("  factor {}: sd {:.4f}, {:5.1f}% of variance,",
                             k + 1, std::sqrt(pca->variances[k]),
                             100.0 * pca->variances[k] / total);
    for (double x : pca->loadings[k]) std::cout << std::format(" {:+.3f}", x);
    std::cout << '\n';
  }
  const std::vector<double> exposures = {10.0, 4.0, -5.0, 2.0};
  const Matrix two = {pca->loadings[0], pca->loadings[1]};
  const std::vector<double> sds = {std::sqrt(pca->variances[0]),
                                   std::sqrt(pca->variances[1])};
  if (const auto sd = r::PcaStdDev(exposures, two, sds)) {
    std::cout << std::format(
        "  exposures (10, 4, -5, 2) per unit move: two-factor sd {:.4f}, "
        "1-day 99% VaR {:.4f}\n",
        *sd, r::NormalVar(*sd, 0.99));
  }
}

}  // namespace

int main() {
  const std::string data = OFOD_DATA_DIR;
  std::vector<double> u;
  for (const auto& row : ReadCsv(data + "/garch_returns.csv")) {
    u.push_back(row[0]);
  }
  LinearModel();
  HistoricalAndBacktest(u);
  QuadraticVsMonteCarlo();
  Pca(ReadCsv(data + "/rate_changes.csv"));
  return 0;
}
