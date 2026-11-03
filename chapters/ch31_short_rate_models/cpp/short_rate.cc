// Copyright 2026 Denis Zhereshchin
//
// Chapter 31: term structures implied by Vasicek and CIR, a Vasicek bond
// option, parameter estimation from simulated short rates and exact CIR
// simulation.

#include "ofod/models/short_rate.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <vector>

#include "ofod/random/normal_generator.h"

namespace {

namespace md = ofod::models;
using ofod::instruments::OptionType;

void TermStructures() {
  const md::Vasicek vasicek{.a = 0.1, .b = 0.05, .sigma = 0.015};
  const md::Cir cir{.a = 0.1, .b = 0.05, .sigma = 0.07};
  std::cout << "Zero rates (%) implied by Vasicek (a = 0.1, b = 5%, sigma = "
               "1.5%) and CIR (sigma = 0.07)\n"
               "  model    r0      1y      2y      5y     10y     30y\n";
  for (double r0 : {0.02, 0.05, 0.08}) {
    for (int m = 0; m < 2; ++m) {
      std::cout << std::format("  {:7} {:4.0f}%", m == 0 ? "Vasicek" : "CIR",
                               100.0 * r0);
      for (double tau : {1.0, 2.0, 5.0, 10.0, 30.0}) {
        const double z = m == 0 ? md::ZeroRate(vasicek, r0, tau)
                                : md::ZeroRate(cir, r0, tau);
        std::cout << std::format("  {:6.3f}", 100.0 * z);
      }
      std::cout << '\n';
    }
  }
  std::cout << std::format(
      "Vasicek 1-year call on a 5-year zero, K = 0.80, r0 = 5%: {:.6f}\n",
      md::ZeroBondOptionPrice(vasicek, OptionType::kCall, 0.05, 1.0, 5.0,
                              0.80));
}

void Estimation() {
  // Ten years of daily rates simulated exactly from a known Vasicek model.
  const md::Vasicek truth{.a = 0.3, .b = 0.04, .sigma = 0.01};
  const double dt = 1.0 / 252.0;
  const double phi = std::exp(-truth.a * dt);
  const double sd =
      truth.sigma * std::sqrt((1.0 - phi * phi) / (2.0 * truth.a));
  ofod::random::NormalGenerator rng(17);
  std::vector<double> rates = {0.03};
  for (int i = 0; i < 252 * 10; ++i) {
    rates.push_back(truth.b + (rates.back() - truth.b) * phi + sd * rng.Next());
  }
  const auto fit = md::EstimateVasicek(rates, dt);
  if (!fit) return;
  std::cout << std::format(
      "Vasicek fit to 10 years of daily rates (true a = 0.3, b = 4%, sigma = "
      "1%): a = {:.4f}, b = {:.4f}%, sigma = {:.4f}%\n",
      fit->a, 100.0 * fit->b, 100.0 * fit->sigma);
}

void CirSimulation() {
  const md::Cir cir{.a = 0.2, .b = 0.05, .sigma = 0.1};
  constexpr int kPaths = 5'000;
  double sum_rate = 0.0;
  double sum_bond = 0.0;
  for (int p = 0; p < kPaths; ++p) {
    const auto path =
        md::SimulateCir(cir, 0.03, 0.05, 100, static_cast<std::uint64_t>(p));
    double integral = 0.0;
    for (std::size_t i = 1; i < path.size(); ++i) {
      integral += 0.5 * (path[i - 1] + path[i]) * 0.05;
    }
    sum_rate += path.back();
    sum_bond += std::exp(-integral);
  }
  std::cout << std::format(
      "CIR exact simulation (a = 0.2, b = 5%, sigma = 0.1, r0 = 3%), 5 years: "
      "E[r] {:.4f}% (theory {:.4f}%), bond {:.5f} (closed form {:.5f})\n",
      100.0 * sum_rate / kPaths,
      100.0 * (0.05 + (0.03 - 0.05) * std::exp(-1.0)), sum_bond / kPaths,
      md::BondPrice(cir, 0.03, 5.0));
}

}  // namespace

int main() {
  try {
    TermStructures();
    Estimation();
    CirSimulation();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
