// Copyright 2026 Denis Zhereshchin
//
// Chapter 33: HJM against Hull-White, the LIBOR market model (caplet
// volatility bootstrap, caplets, ratchet and sticky caps, Rebonato's
// swaption volatility) and agency MBS with PSA and rate-driven
// prepayments.

#include <algorithm>
#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/models/lmm.h"
#include "ofod/models/short_rate.h"
#include "ofod/pricers/mbs.h"
#include "ofod/pricers/rate_options.h"

namespace {

namespace md = ofod::models;
namespace p = ofod::pricers;
using ofod::curves::ZeroCurve;
using ofod::instruments::OptionType;

// Unwraps a result the example inputs guarantee; main reports a failure.
template <typename T>
T Must(std::optional<T> value) {
  if (!value) throw std::runtime_error("unexpected pricing failure");
  return *std::move(value);
}

constexpr int kPaths = 10'000;

void Hjm() {
  const ZeroCurve curve = Must(ZeroCurve::Create({1.0, 5.0}, {0.03, 0.05}));
  const double dt = 0.05;
  md::HjmSpec spec{.dt = dt, .forwards = {}, .volatility = nullptr};
  for (int i = 0; i < 100; ++i) {
    spec.forwards.push_back(std::log(curve.DiscountFactor(i * dt) /
                                     curve.DiscountFactor((i + 1) * dt)) /
                            dt);
  }
  spec.volatility = [](double t, double maturity) {
    return 0.01 * std::exp(-0.1 * (maturity - t));
  };
  const auto bond =
      md::HjmZeroBondOption(spec, OptionType::kCall, 40, 100, 0.0, kPaths, 1);
  const auto put =
      md::HjmZeroBondOption(spec, OptionType::kPut, 40, 100, 0.82, kPaths, 2);
  const auto hw = md::HullWhite::Create(curve, 0.1, 0.01);
  if (!bond || !put || !hw) return;
  std::cout << std::format(
      "HJM with v(t, T) = 0.01 e^(-0.1 (T - t)), dt = 0.05: 5-year zero "
      "{:.5f} (curve {:.5f}); 2-year put on it at 0.82 {:.5f} (se {:.5f}), "
      "Hull-White {:.5f}\n",
      bond->price, curve.DiscountFactor(5.0), put->price, put->std_error,
      hw->ZeroBondOption(OptionType::kPut, 2.0, 5.0, 0.82));
}

md::LmmSpec Spec(const std::vector<double>& lambdas, int factors) {
  md::LmmSpec spec{.accrual = 1.0,
                   .forwards = std::vector<double>(10, 0.05),
                   .loadings = {}};
  for (std::size_t i = 0; i < lambdas.size(); ++i) {
    if (factors == 1) {
      spec.loadings.push_back({lambdas[i]});
    } else {
      const double angle = 0.2 * static_cast<double>(i);
      spec.loadings.push_back(
          {lambdas[i] * std::cos(angle), lambdas[i] * std::sin(angle)});
    }
  }
  return spec;
}

void Lmm() {
  const auto hull =
      md::LmmVolatilitiesFromCaplets(std::vector<double>{0.24, 0.22, 0.20});
  if (!hull) return;
  std::cout << std::format(
      "Caplet vols 24%, 22%, 20% give Lambda = {:.2f}%, {:.2f}%, {:.2f}%\n",
      100.0 * (*hull)[0], 100.0 * (*hull)[1], 100.0 * (*hull)[2]);
  const std::vector<double> caplet_vols = {
      0.155, 0.1825, 0.1791, 0.1774, 0.1727, 0.1679, 0.163, 0.1601, 0.1576};
  const auto lambdas = md::LmmVolatilitiesFromCaplets(caplet_vols);
  if (!lambdas) return;
  std::cout << "LMM: 10 annual forwards at 5%, Lambda (%):";
  for (double l : *lambdas) std::cout << std::format(" {:.2f}", 100.0 * l);
  std::cout << '\n';

  // Caps with a 25 bp spread, per 100 notional, paid in arrears.
  const md::LmmPayoff plain = [](const md::LmmPath& path) {
    double value = 0.0;
    for (int k = 1; k < path.size(); ++k) {
      value += std::max(path.Reset(k) - 0.05, 0.0) / path.Numeraire(k + 1);
    }
    return 100.0 * value;
  };
  const md::LmmPayoff ratchet = [](const md::LmmPath& path) {
    double value = 0.0;
    for (int k = 1; k < path.size(); ++k) {
      const double strike = path.Reset(k - 1) + 0.0025;
      value += std::max(path.Reset(k) - strike, 0.0) / path.Numeraire(k + 1);
    }
    return 100.0 * value;
  };
  const md::LmmPayoff sticky = [](const md::LmmPath& path) {
    double value = 0.0;
    double capped = path.Reset(0);
    for (int k = 1; k < path.size(); ++k) {
      const double strike = capped + 0.0025;
      value += std::max(path.Reset(k) - strike, 0.0) / path.Numeraire(k + 1);
      capped = std::min(path.Reset(k), strike);
    }
    return 100.0 * value;
  };
  const md::LmmSpec one = Spec(*lambdas, 1);
  const std::vector<double> df = md::LmmDiscountFactors(one);
  double black = 0.0;
  for (int k = 1; k < 10; ++k) {
    black +=
        p::BlackCapletPrice(OptionType::kCall, 0.05, 0.05,
                            caplet_vols[static_cast<std::size_t>(k - 1)], k,
                            1.0, df[static_cast<std::size_t>(k) + 1], 100.0);
  }
  std::cout << std::format(
      "  caps per 100 (spot measure, predictor-corrector, {} paths); Black "
      "ATM cap {:.4f}\n"
      "  factors  ATM cap   ratchet  sticky\n",
      kPaths, black);
  for (int factors : {1, 2}) {
    const md::LmmSpec spec = Spec(*lambdas, factors);
    const md::LmmOptions options{
        .paths = kPaths, .seed = 11, .predictor_corrector = true};
    const auto a = md::LmmMonteCarlo(spec, plain, options);
    const auto b = md::LmmMonteCarlo(spec, ratchet, options);
    const auto c = md::LmmMonteCarlo(spec, sticky, options);
    if (!a || !b || !c) return;
    std::cout << std::format("  {:7}  {:7.4f}  {:7.4f}  {:7.4f}\n", factors,
                             a->price, b->price, c->price);
  }

  // A 3 x 5 European payer swaption, two factors.
  const md::LmmSpec two = Spec(*lambdas, 2);
  const int start = 3;
  const int end = 8;
  const auto vol = md::RebonatoSwaptionVolatility(two, start, end);
  double annuity = 0.0;
  for (int k = start; k < end; ++k) {
    annuity += df[static_cast<std::size_t>(k) + 1];
  }
  const double s0 = (df[start] - df[end]) / annuity;
  const md::LmmPayoff swaption = [s0](const md::LmmPath& path) {
    double discount = 1.0;
    double a = 0.0;
    for (int k = start; k < end; ++k) {
      discount = discount / (1.0 + path.Forward(start, k));
      a += discount;
    }
    return 100.0 * std::max(1.0 - discount - s0 * a, 0.0) /
           path.Numeraire(start);
  };
  const auto mc = md::LmmMonteCarlo(
      two, swaption,
      {.paths = kPaths, .seed = 12, .predictor_corrector = true});
  if (!vol || !mc) return;
  std::cout << std::format(
      "3 x 5 ATM payer swaption per 100: Rebonato vol {:.2f}% gives Black "
      "{:.4f}; LMM Monte Carlo {:.4f} (se {:.4f})\n",
      100.0 * *vol,
      p::BlackSwaptionPrice(OptionType::kCall, s0, s0, *vol, start, annuity,
                            100.0),
      mc->price, mc->std_error);
}

void Mbs() {
  const p::MortgagePool pool{.balance = 100.0,
                             .mortgage_rate = 0.06,
                             .pass_through_rate = 0.055,
                             .months = 360};
  const ZeroCurve flat = ZeroCurve::Flat(0.05);
  std::cout << "MBS: 30-year pool, WAC 6%, pass-through 5.5%, flat 5% curve\n"
               "  PSA   pass-through      IO      PO\n";
  for (double speed : {0.0, 50.0, 100.0, 200.0, 400.0}) {
    const auto v =
        p::ValueCashFlows(p::PassThroughCashFlows(pool, speed), flat);
    std::cout << std::format("  {:3.0f}   {:12.4f}  {:6.2f}  {:6.2f}\n", speed,
                             v.pass_through, v.interest_only, v.principal_only);
  }
  std::cout << "  Monte Carlo with refinancing (Hull-White a = 0.1, sigma = "
               "1%, 2,000 paths)\n"
               "  rates   pass-through      IO      PO\n";
  for (double shift : {-0.01, 0.0, 0.01}) {
    const auto hw =
        md::HullWhite::Create(ZeroCurve::Flat(0.05 + shift), 0.1, 0.01);
    if (!hw) return;
    const auto v = p::MbsMonteCarlo(pool, *hw, {}, {.paths = 2'000, .seed = 3});
    if (!v) return;
    std::cout << std::format("  {:4.0f}%   {:12.4f}  {:6.2f}  {:6.2f}\n",
                             100.0 * (0.05 + shift), v->pass_through,
                             v->interest_only, v->principal_only);
  }
}

}  // namespace

int main() {
  try {
    Hjm();
    Lmm();
    Mbs();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
