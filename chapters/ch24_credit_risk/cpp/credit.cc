// Copyright 2026 Denis Zhereshchin
//
// Chapter 24: hazard rates from spreads, a piecewise credit curve, Merton's
// model, Gaussian-copula joint defaults and the Vasicek credit VaR.

#include <cmath>
#include <format>
#include <iostream>
#include <vector>

#include "ofod/credit/credit_risk.h"
#include "ofod/curves/credit_curve.h"

namespace {

namespace c = ofod::credit;
using ofod::curves::CreditCurve;

void HazardRates() {
  std::cout << std::format("200 bp spread, 40% recovery: hazard rate {:.4f}\n",
                           ofod::curves::HazardFromSpread(0.02, 0.4));
  std::cout << std::format(
      "5% cumulative default by year 5: average hazard {:.6f}\n",
      ofod::curves::AverageHazard(0.05, 5.0));
  const auto curve = CreditCurve::Create({2.0, 5.0}, {0.01, 0.02});
  if (!curve) return;
  std::cout << "Hazards 1% to year 2, 2% to year 5\n";
  std::cout << "  year  survival  default in year\n";
  for (int t = 1; t <= 5; ++t) {
    std::cout << std::format("  {:4d}  {:8.6f}  {:15.6f}\n", t,
                             curve->SurvivalProbability(t),
                             curve->DefaultProbability(t - 1.0, t));
  }
}

void Merton() {
  // E0 = 3, sigma_E = 80%, D = 10, r = 5%, T = 1 (Hull, Example 24.3).
  const auto m = c::MertonSolve(3.0, 0.8, 10.0, 0.05, 1.0);
  if (!m) return;
  const double riskless = 10.0 * std::exp(-0.05);
  const double spread = -std::log(m->debt_value / 10.0) - 0.05;
  std::cout << "Merton: E0 = 3, sigma_E = 80%, D = 10, r = 5%, T = 1\n";
  std::cout << std::format(
      "  V0 {:.4f}  sigma_V {:.4f}  P(default) {:.4f}  debt {:.4f} vs "
      "riskless {:.4f}\n",
      m->asset_value, m->asset_volatility, m->default_probability,
      m->debt_value, riskless);
  std::cout << std::format("  expected loss {:.2f}%  credit spread {:.1f} bp\n",
                           100.0 * (1.0 - m->debt_value / riskless),
                           1e4 * spread);
}

void Copula() {
  std::cout << "Gaussian copula, Q1 = 1%, Q2 = 2%\n";
  for (double rho : {0.0, 0.3, 0.6, 0.9}) {
    std::cout << std::format("  rho {:.1f}: P(both default) {:.6f}\n", rho,
                             c::JointDefaultProbability(0.01, 0.02, rho));
  }
  std::cout << "Conditional default probability, Q = 2%, rho = 0.1\n";
  for (double f : {-3.0, -2.0, 0.0, 2.0}) {
    std::cout << std::format("  F = {:+.0f}: {:.6f}\n", f,
                             c::ConditionalDefaultProbability(0.02, 0.1, f));
  }
}

void CreditVar() {
  // $100M retail loans, PD 2%, LGD 40%, rho = 0.1 (Hull, Example 24.5).
  const double wcdr = c::Wcdr(0.02, 0.1, 0.999);
  const double loss = c::VasicekCreditLoss(100.0, 0.02, 0.4, 0.1, 0.999);
  const double expected = 100.0 * 0.02 * 0.4;
  std::cout << std::format(
      "Vasicek: WCDR(99.9%) {:.4f}, loss {:.2f}M, expected {:.2f}M, credit "
      "VaR {:.2f}M\n",
      wcdr, loss, expected, loss - expected);
}

}  // namespace

int main() {
  HazardRates();
  Merton();
  Copula();
  CreditVar();
  return 0;
}
