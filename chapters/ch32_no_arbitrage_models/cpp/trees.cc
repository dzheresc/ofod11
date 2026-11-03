// Copyright 2026 Denis Zhereshchin
//
// Chapter 32: Hull's trinomial tree for the Hull-White model, tree prices
// converging to the analytic bond options, Jamshidian's decomposition,
// European and Bermudan swaptions under Hull-White and Black-Karasinski,
// and calibration to caplet prices.

#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/models/short_rate.h"
#include "ofod/pricers/fra_pricer.h"
#include "ofod/pricers/rate_options.h"
#include "ofod/pricers/short_rate_tree.h"

namespace {

namespace md = ofod::models;
namespace p = ofod::pricers;
using ofod::curves::ZeroCurve;
using ofod::instruments::Exercise;
using ofod::instruments::OptionType;

// Unwraps a result the example inputs guarantee; main reports a failure.
template <typename T>
T Must(std::optional<T> value) {
  if (!value) throw std::runtime_error("unexpected pricing failure");
  return *std::move(value);
}

ZeroCurve HullCurve() {
  return Must(
      ZeroCurve::Create({0.5, 1.0, 1.5, 2.0, 2.5, 3.0},
                        {0.0343, 0.03824, 0.04183, 0.04512, 0.04812, 0.05086}));
}

void HullTree(const ZeroCurve& curve) {
  const auto tree = p::ShortRateTree::Build(curve, 0.1, 0.01, 1.0, 3);
  if (!tree) return;
  std::cout << std::format(
      "Hull-White tree, a = 0.1, sigma = 0.01, dt = 1: dx = {:.5f}, jmax = "
      "{}\n  alphas:",
      tree->dx(), tree->jmax());
  for (double a : tree->alphas()) std::cout << std::format(" {:.3f}%", 100 * a);
  std::cout << "\n  node   pu      pm      pd\n";
  for (int j = 2; j >= -2; --j) {
    const p::Branch& b = tree->BranchAt(j);
    std::cout << std::format("  {:+d}   {:.4f}  {:.4f}  {:.4f}\n", j, b.up,
                             b.mid, b.down);
  }
  std::cout << "  rates at step 2 (%):";
  for (int j = -2; j <= 2; ++j) {
    std::cout << std::format(" {:.3f}", 100.0 * tree->Rate(2, j));
  }
  std::cout << '\n';
}

void Convergence(const ZeroCurve& curve, const md::HullWhite& hw) {
  const double analytic = hw.ZeroBondOption(OptionType::kPut, 1.0, 3.0, 0.92);
  std::cout << std::format(
      "1-year put on a 3-year zero, K = 0.92: analytic {:.6f}\n"
      "  dt      tree       error\n",
      analytic);
  for (int n : {4, 10, 20, 50, 100}) {
    const double dt = 1.0 / n;
    const auto tree = p::ShortRateTree::Build(curve, 0.1, 0.01, dt, 3 * n);
    if (!tree) throw std::runtime_error("tree construction failed");
    const std::vector<p::TreeCashFlow> zero = {{3 * n, 1.0}};
    const double v = Must(p::TreeBondOption(
        *tree, OptionType::kPut, Exercise::kEuropean, n, zero, 0.92));
    std::cout << std::format("  {:.3f}  {:.6f}  {:+.6f}\n", dt, v,
                             v - analytic);
  }
  const auto jamshidian =
      hw.CouponBondOption(OptionType::kCall, 1.0, std::vector<double>{2.0, 3.0},
                          std::vector<double>{5.0, 105.0}, 99.0);
  const auto tree = p::ShortRateTree::Build(curve, 0.1, 0.01, 0.01, 300);
  if (!tree) throw std::runtime_error("tree construction failed");
  const std::vector<p::TreeCashFlow> bond = {{200, 5.0}, {300, 105.0}};
  std::cout << std::format(
      "Call on a 3-year 5% bond, K = 99, in 1 year: Jamshidian {:.4f}, tree "
      "(dt = 0.01) {:.4f}, American on the tree {:.4f}\n",
      jamshidian.value_or(0.0),
      Must(p::TreeBondOption(*tree, OptionType::kCall, Exercise::kEuropean, 100,
                             bond, 99.0)),
      Must(p::TreeBondOption(*tree, OptionType::kCall, Exercise::kAmerican, 100,
                             bond, 99.0)));
}

void Swaptions(const ZeroCurve& curve) {
  // Receiver swaption into an annual swap ending in 3 years, K = 5%.
  const std::vector<int> european = {50};
  const std::vector<int> bermudan = {50, 100};
  std::cout << "Receiver swaptions, annual 5% swap ending in 3 years, dt = "
               "0.02\n"
               "  model                 European  Bermudan\n";
  for (int m = 0; m < 2; ++m) {
    const auto model = m == 0 ? p::ShortRateModel::kHullWhite
                              : p::ShortRateModel::kBlackKarasinski;
    const double sigma = m == 0 ? 0.01 : 0.25;
    const auto tree =
        p::ShortRateTree::Build(curve, 0.1, sigma, 0.02, 150, model);
    if (!tree) continue;
    std::cout << std::format(
        "  {:20}  {:8.5f}  {:8.5f}\n",
        m == 0 ? "Hull-White 1%" : "Black-Karasinski 25%",
        Must(p::TreeSwaption(*tree, OptionType::kPut, 0.05, 50, 150, european)),
        Must(
            p::TreeSwaption(*tree, OptionType::kPut, 0.05, 50, 150, bermudan)));
  }
}

void Calibration(const ZeroCurve& curve) {
  // Semiannual caplets priced with Black vols falling from 29% to 17%
  // (the shape Hull-White produces) for two strikes.
  std::vector<md::CapletQuote> quotes;
  for (double reset : {0.5, 1.0, 1.5, 2.0, 2.5}) {
    const double forward = p::SimpleForwardRate(curve, reset, reset + 0.5);
    for (double strike : {0.04, 0.06}) {
      quotes.push_back(
          {reset, 0.5, strike,
           p::BlackCapletPrice(OptionType::kCall, forward, strike,
                               0.32 - 0.06 * reset, reset, 0.5,
                               curve.DiscountFactor(reset + 0.5), 1.0)});
    }
  }
  const auto fit = md::CalibrateHullWhite(curve, quotes);
  if (!fit) return;
  std::cout << std::format(
      "Hull-White calibrated to 10 caplets with falling Black vols: a = "
      "{:.4f}, "
      "sigma = {:.4f}%, RMS relative error {:.2f}%\n",
      fit->a, 100.0 * fit->sigma, 100.0 * fit->rms_relative_error);
}

}  // namespace

int main() {
  try {
    const ZeroCurve curve = HullCurve();
    const auto hw = md::HullWhite::Create(curve, 0.1, 0.01);
    if (!hw) return 1;
    HullTree(curve);
    Convergence(curve, *hw);
    Swaptions(curve);
    Calibration(curve);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
