// Copyright 2026 Denis Zhereshchin
//
// Chapter 37: three lessons replayed with the library. A
// Metallgesellschaft-style stack-and-roll hedge whose futures losses are
// paid in cash while the offsetting gain on the delivery contracts is not;
// model risk from delta hedging options sold at too low a volatility; and
// a position-limit monitor that would have stopped a trader doubling up.

#include <algorithm>
#include <cmath>
#include <exception>
#include <format>
#include <iostream>

#include "ofod/instruments/option.h"
#include "ofod/math/normal.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/random/normal_generator.h"
#include "ofod/risk/var.h"

namespace {

namespace p = ofod::pricers;
using ofod::instruments::OptionType;

// Spot falls linearly from 20 to 15 over a year, then stays flat; the
// market is in contango by `kCarry` a month.
constexpr double kCarry = 0.1;
constexpr int kDeliveries = 60;
constexpr double kFixedPrice = 22.0;
constexpr double kRate = 0.05;

double Spot(int month) {
  return month >= 12 ? 15.0 : 20.0 - 5.0 * month / 12.0;
}

// Value at `month` of the remaining fixed-price delivery commitments, each
// worth (fixed - forward) discounted, with forward = spot + carry * k.
double CommitmentValue(int month) {
  double value = 0.0;
  for (int k = 1; k <= kDeliveries - month; ++k) {
    value += (kFixedPrice - (Spot(month) + kCarry * k)) *
             std::exp(-kRate * k / 12.0);
  }
  return value;
}

void StackAndRoll() {
  std::cout << "Stack-and-roll: deliver 1 unit a month for 5 years at 22, "
               "hedged with a rolled stack of 1-month futures\n";
  std::cout << "  month   spot   futures cash   commitments   net\n";
  const double start = CommitmentValue(0);
  double cash = 0.0;
  for (int month = 0; month <= 12; ++month) {
    if (month > 0) {
      // Last month's stack (one unit per remaining delivery) bought at
      // spot + carry settles at this month's spot.
      const int stack = kDeliveries - (month - 1);
      cash = cash * std::exp(kRate / 12.0) +
             stack * (Spot(month) - (Spot(month - 1) + kCarry));
    }
    if (month % 3 == 0) {
      const double gain = CommitmentValue(month) - start;
      std::cout << std::format(
          "  {:5d}  {:5.2f}  {:13.2f}  {:12.2f}  {:6.2f}\n", month, Spot(month),
          cash, gain, cash + gain);
    }
  }
}

void ModelRisk() {
  // Sell a one-year at-the-money call at 15% implied volatility when the
  // stock's true volatility is 25%, and delta hedge weekly at 15%.
  const double s0 = 100.0;
  const double r = 0.05;
  const double sold_vol = 0.15;
  const double true_vol = 0.25;
  const int steps = 52;
  const int paths = 2000;
  const double dt = 1.0 / steps;
  const double premium =
      p::BsmPrice(OptionType::kCall, s0, s0, r, 0.0, sold_vol, 1.0);
  const double fair =
      p::BsmPrice(OptionType::kCall, s0, s0, r, 0.0, true_vol, 1.0);
  auto delta = [&](double s, double tau) {
    const double d1 =
        (std::log(s / s0) + (r + 0.5 * sold_vol * sold_vol) * tau) /
        (sold_vol * std::sqrt(tau));
    return ofod::math::NormCdf(d1);
  };
  ofod::random::NormalGenerator rng(37);
  double sum = 0.0;
  double sum_sq = 0.0;
  for (int path = 0; path < paths; ++path) {
    double s = s0;
    double held = delta(s, 1.0);
    double bank = premium - held * s;
    for (int i = 1; i <= steps; ++i) {
      s *= std::exp((r - 0.5 * true_vol * true_vol) * dt +
                    true_vol * std::sqrt(dt) * rng.Next());
      bank *= std::exp(r * dt);
      if (i < steps) {
        const double next = delta(s, 1.0 - i * dt);
        bank -= (next - held) * s;
        held = next;
      }
    }
    const double pnl = std::exp(-r) * (bank + held * s - std::max(s - s0, 0.0));
    sum += pnl;
    sum_sq += pnl * pnl;
  }
  const double mean = sum / paths;
  const double sd = std::sqrt(sum_sq / paths - mean * mean);
  std::cout << std::format(
      "Model risk: one-year ATM call sold at 15% vol ({:.4f}) when the true "
      "vol is 25% ({:.4f})\n  weekly delta hedging at 15%: mean P&L {:.4f} "
      "(sd {:.4f}); expected loss from the price gap {:.4f}\n",
      premium, fair, mean, sd, premium - fair);
}

void PositionLimits() {
  // A trader long an index future doubles the position after each losing
  // day while the market falls 1% a day; daily volatility is 1.5%.
  const double daily_vol = 0.015;
  const double limit = 5.0;  // 99% one-day VaR limit, $m.
  double position = 10.0;    // $m.
  double limited = 10.0;
  double loss = 0.0;
  double limited_loss = 0.0;
  int breach = -1;
  for (int day = 1; day <= 10; ++day) {
    loss += 0.01 * position;
    limited_loss += 0.01 * limited;
    position *= 2.0;
    const double var = ofod::risk::NormalVar(daily_vol * position, 0.99);
    if (var > limit && breach < 0) breach = day;
    // The monitored desk is held at the largest position within the limit.
    const double max_position = limit / (daily_vol * ofod::math::NormInv(0.99));
    limited = std::min(2.0 * limited, max_position);
  }
  std::cout << std::format(
      "Position limits: doubling up in a market falling 1% a day for 10 "
      "days\n  VaR limit of $5m first breached on day {}; loss {:.2f} $m "
      "unchecked vs {:.2f} $m with the limit enforced\n",
      breach, loss, limited_loss);
}

}  // namespace

int main() {
  try {
    StackAndRoll();
    ModelRisk();
    PositionLimits();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
