// Copyright 2026 Denis Zhereshchin
//
// Chapter 35: a seasonal gas curve, Hull's mean-reverting commodity tree,
// electricity price spikes, degree-day options by burn analysis and by
// simulation, a catastrophe bond, a reinsurance layer and a producer's
// hedge.

#include <algorithm>
#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ofod/instruments/option.h"
#include "ofod/models/commodity.h"
#include "ofod/pricers/commodity_tree.h"
#include "ofod/pricers/weather_insurance.h"

namespace {

namespace md = ofod::models;
namespace p = ofod::pricers;
using ofod::instruments::Exercise;
using ofod::instruments::OptionType;

// Unwraps a result the example inputs guarantee; main reports a failure.
template <typename T>
T Must(std::optional<T> value) {
  if (!value) throw std::runtime_error("unexpected pricing failure");
  return *std::move(value);
}

// Synthetic daily temperatures (deterministic, no random numbers).
std::vector<double> SampleTemperatures(int days) {
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(days));
  for (int d = 0; d < days; ++d) {
    out.push_back(52.0 +
                  22.0 * std::cos(2.0 * std::numbers::pi * (d - 200) / 365.0) +
                  4.0 * std::sin(0.9 * d) + 3.0 * std::cos(2.3 * d + 1.0));
  }
  return out;
}

void Seasonal() {
  std::vector<double> t;
  std::vector<double> f;
  for (int k = 0; k < 24; ++k) {
    t.push_back((k + 0.5) / 12.0);
    f.push_back(3.0 * std::exp(0.03 * t.back() +
                               0.18 * std::cos(2.0 * std::numbers::pi *
                                               (t.back() - 1.0 / 24.0))));
  }
  const md::SeasonalCurve curve = Must(md::FitSeasonalCurve(t, f));
  std::cout << std::format(
      "Seasonal gas curve from 24 monthly futures: level {:.4f}, growth "
      "{:.4f}\n  monthly multipliers:",
      std::exp(curve.level), curve.growth);
  for (int m = 0; m < 12; ++m) {
    std::cout << std::format(" {:.3f}", curve.Multiplier((m + 0.5) / 12.0));
  }
  std::cout << '\n';
}

void Tree() {
  const std::vector<double> futures = {22.0, 23.0, 24.0};
  const auto tree = p::CommodityTree::Build(20.0, futures, 0.1, 0.2, 1.0);
  if (!tree) throw std::runtime_error("tree construction failed");
  std::cout << std::format(
      "Commodity tree, S = 20, futures 22, 23, 24, a = 0.1, sigma = 0.2: dx "
      "= {:.5f}, jmax = {}\n  alphas:",
      tree->dx(), tree->jmax());
  for (double a : tree->alphas()) std::cout << std::format(" {:.5f}", a);
  std::cout << "\n  prices at year 3:";
  const int w = tree->Width(3);
  for (int j = -w; j <= w; ++j) {
    std::cout << std::format(" {:.4f}", tree->Price(3, j));
  }
  std::cout << std::format(
      "\n  3-year put, K = 22, r = 5%: European {:.4f}, American {:.4f}\n",
      Must(p::CommodityTreeOption(*tree, OptionType::kPut, Exercise::kEuropean,
                                  22.0, 3, 0.05)),
      Must(p::CommodityTreeOption(*tree, OptionType::kPut, Exercise::kAmerican,
                                  22.0, 3, 0.05)));
}

void Spikes() {
  const md::SpikeModel model{.log_level = std::log(40.0),
                             .a = 8.0,
                             .sigma = 0.8,
                             .intensity = 6.0,
                             .jump_mean = 0.8,
                             .jump_vol = 0.3};
  const md::SpikeModel calm{
      .log_level = std::log(40.0), .a = 8.0, .sigma = 0.8};
  std::cout << std::format(
      "Electricity, $40 level, a = 8, 6 spikes a year: E[S(3 months)] "
      "{:.4f} (no spikes {:.4f})\n",
      Must(md::SpikeExpectedPrice(model, 0.0, 0.25)),
      Must(md::SpikeExpectedPrice(calm, 0.0, 0.25)));
  const auto mc = Must(md::SpikeMonteCarlo(model, 0.0, 0.25, 50, 20'000, 1));
  const auto path = Must(md::SimulateSpikePath(model, 0.0, 1.0 / 365, 365, 2));
  std::cout << std::format(
      "  Monte Carlo {:.4f} (se {:.4f}); one simulated year peaks at "
      "{:.2f}\n",
      mc.mean, mc.std_error, *std::ranges::max_element(path));
}

void Weather() {
  const std::vector<double> week = {38.0, 45.0, 52.0, 61.0, 66.0, 70.0, 59.0};
  std::cout << std::format("Degree days for a week: HDD {:.1f}, CDD {:.1f}\n",
                           p::DegreeDays(p::DegreeDayKind::kHeating, week),
                           p::DegreeDays(p::DegreeDayKind::kCooling, week));
  const p::DegreeDayOption call{.type = OptionType::kCall,
                                .strike = 700.0,
                                .tick = 10'000.0,
                                .cap = 2'000'000.0};
  const std::vector<double> seasons = {650.0, 720.0, 805.0, 690.0, 760.0,
                                       880.0, 610.0, 702.0, 745.0, 698.0};
  const auto burn = Must(p::BurnAnalysis(call, seasons, 0.05, 0.5));
  std::cout << std::format(
      "HDD call, K = 700, $10,000 a degree day, cap $2m: burn analysis over "
      "10 seasons {:.0f} (se {:.0f})\n",
      burn.value, burn.std_error);
  const std::vector<double> temps = SampleTemperatures(365 * 6);
  const auto model = Must(p::FitTemperatureModel(temps));
  std::cout << std::format(
      "Temperature model from 6 years of daily data: mean {:.2f}, amplitude "
      "{:.2f}, peak day {:.1f}, phi {:.3f}, sigma {:.3f}\n",
      model.mean, model.amplitude, model.phase, model.phi, model.sigma);
  // December to February after the six years of data.
  const p::SeasonSpec winter{.kind = p::DegreeDayKind::kHeating,
                             .first_day = 365 * 6 + 334,
                             .days = 90};
  const p::DegreeDayOption winter_call{
      .type = OptionType::kCall, .strike = 2'400.0, .tick = 5'000.0};
  const auto sim = Must(p::SimulateDegreeDayOption(model, winter, winter_call,
                                                   0.05, 0.5, 20'000, 3));
  std::cout << std::format(
      "  winter HDD simulated: mean {:.1f}, sd {:.1f}; call at 2400, $5,000 "
      "a degree day: {:.0f} (se {:.0f})\n",
      sim.mean_degree_days, sim.std_degree_days, sim.option.value,
      sim.option.std_error);
}

void Insurance() {
  const p::LossModel hurricanes{
      .intensity = 0.3, .severity_mu = std::log(300.0), .severity_sigma = 0.8};
  const p::CatBondTerms bond{.principal = 100.0,
                             .coupon = 0.08,
                             .maturity = 3.0,
                             .attachment = 500.0,
                             .exhaustion = 1'000.0,
                             .frequency = 2};
  const auto cat = Must(p::PriceCatBond(bond, hurricanes, 0.03, 100'000, 4));
  std::cout << std::format(
      "Cat bond, 8% coupon, 3 years, principal lost between $500m and $1bn "
      "of losses:\n  price {:.4f} (se {:.4f}), P(loss) {:.4f}, expected "
      "loss {:.4f}, spread {:.0f} bp\n",
      cat.price, cat.std_error, cat.loss_probability, cat.expected_loss,
      1e4 * cat.spread);
  const auto layer = Must(
      p::ExcessOfLossLayer(hurricanes, 500.0, 500.0, 1.0, 0.03, 100'000, 5));
  std::cout << std::format(
      "  one-year excess-of-loss layer, $500m xs $500m: expected cost "
      "{:.4f} (se {:.4f})\n",
      layer.value, layer.std_error);
}

void ProducerHedge() {
  // A producer sells 1 million barrels in a year at the then spot price;
  // hedges: none, a forward sale at 70, or a 60-80 collar.
  std::cout << "Producer's revenue ($m) for 1m barrels: spot, unhedged, "
               "forward at 70, 60-80 collar\n";
  for (double spot : {50.0, 60.0, 70.0, 80.0, 90.0}) {
    std::cout << std::format("  {:5.1f}  {:6.1f}  {:6.1f}  {:6.1f}\n", spot,
                             spot, 70.0, std::clamp(spot, 60.0, 80.0));
  }
}

}  // namespace

int main() {
  try {
    Seasonal();
    Tree();
    Spikes();
    Weather();
    Insurance();
    ProducerHedge();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
