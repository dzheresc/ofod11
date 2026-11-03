// Copyright 2026 Denis Zhereshchin
//
// Chapter 29: Black's model for bond options, caps, floors and swaptions,
// caplet volatility stripping, the normal model and bucketed DV01s.

#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/pricers/fra_pricer.h"
#include "ofod/pricers/rate_options.h"

namespace {

namespace p = ofod::pricers;
using ofod::curves::ZeroCurve;
using p::OptionType;

// Unwraps a result the example inputs guarantee; main reports a failure.
template <typename T>
T Must(std::optional<T> value) {
  if (!value) throw std::runtime_error("unexpected pricing failure");
  return *std::move(value);
}

ZeroCurve Upward() {
  return Must(ZeroCurve::Create({0.25, 1.0, 2.0, 5.0, 10.0},
                                {0.03, 0.035, 0.04, 0.045, 0.05}));
}

void BookExamples() {
  const double coupons =
      50.0 * std::exp(-0.25 * 0.09) + 50.0 * std::exp(-0.75 * 0.095);
  const double discount = std::exp(-0.1 * 10.0 / 12.0);
  const double forward = p::ForwardBondPrice(960.0, coupons, discount);
  std::cout << std::format(
      "Example 29.1 bond option: coupons PV {:.2f}, forward price {:.2f}, "
      "call {:.2f}\n",
      coupons, forward,
      p::BlackBondOptionPrice(OptionType::kCall, forward, 1000.0, discount,
                              0.09, 10.0 / 12.0));
  std::cout << std::format(
      "Example 29.2 caplet: L = 10,000, F = 7%, K = 8%, 20% vol: {:.2f}\n",
      p::BlackCapletPrice(OptionType::kCall, 0.07, 0.08, 0.2, 1.0, 0.25,
                          std::exp(-0.065 * 1.25), 10'000.0));
  const auto swap = p::ForwardSwapRate(ZeroCurve::Flat(0.06), 5.0, 3.0, 2);
  std::cout << std::format(
      "Example 29.3 swaption: annuity {:.4f}, forward swap rate {:.4f}%, "
      "payer at 6.2% on $100M {:.4f}M\n",
      swap.annuity, 100.0 * swap.rate,
      p::BlackSwaptionPrice(OptionType::kCall, swap.rate, 0.062, 0.2, 5.0,
                            swap.annuity, 100.0));
}

void CapsAndStripping(const ZeroCurve& curve) {
  const p::CapTerms terms{.strike = 0.04, .maturity = 5.0, .frequency = 4};
  const double cap =
      Must(p::CapFloorPrice(OptionType::kCall, curve, terms, 0.2));
  const double floor =
      Must(p::CapFloorPrice(OptionType::kPut, curve, terms, 0.2));
  double swap = 0.0;
  for (int k = 1; k < 20; ++k) {
    const double reset = 0.25 * k;
    swap += 0.25 * (p::SimpleForwardRate(curve, reset, reset + 0.25) - 0.04) *
            curve.DiscountFactor(reset + 0.25);
  }
  std::cout << std::format(
      "5-year quarterly cap and floor at 4%, 20% vol, per 100: cap {:.4f}, "
      "floor {:.4f}, cap - floor {:.4f} = swap {:.4f}\n",
      100.0 * cap, 100.0 * floor, 100.0 * (cap - floor), 100.0 * swap);
  const std::vector<double> flat = {0.18, 0.2,   0.21, 0.21, 0.2,
                                    0.19, 0.185, 0.18, 0.175};
  const auto spot = p::StripCapletVolatilities(curve, 0.04, 2, flat);
  if (!spot) return;
  std::cout << "Caplet vol stripping (semiannual, K = 4%)\n"
               "  cap maturity  flat vol  spot vol\n";
  for (std::size_t i = 0; i < flat.size(); ++i) {
    std::cout << std::format("  {:12.1f}  {:7.2f}%  {:7.2f}%\n",
                             0.5 * static_cast<double>(i + 2), 100.0 * flat[i],
                             100.0 * Must(spot)[i]);
  }
}

void SwaptionGrid(const ZeroCurve& curve) {
  std::cout << "ATM payer swaptions per 100 notional, 20% vol, semiannual\n"
               "  expiry   2y swap   5y swap  10y swap\n";
  for (double expiry : {1.0, 2.0, 5.0}) {
    std::cout << std::format("  {:5.0f}y", expiry);
    for (double tenor : {2.0, 5.0, 10.0}) {
      const auto s = p::ForwardSwapRate(curve, expiry, tenor, 2);
      std::cout << std::format(
          "  {:8.4f}", p::BlackSwaptionPrice(OptionType::kCall, s.rate, s.rate,
                                             0.2, expiry, s.annuity, 100.0));
    }
    std::cout << '\n';
  }
}

void NormalModel() {
  // Rates near zero: Black cannot price a negative forward, Bachelier can.
  const ZeroCurve negative =
      Must(ZeroCurve::Create({1.0, 10.0}, {-0.005, 0.002}));
  const auto s = p::ForwardSwapRate(negative, 2.0, 5.0, 1);
  std::cout << std::format(
      "Normal model: 2y x 5y forward swap rate {:.4f}%, receiver at 0% with "
      "50 bp normal vol {:.4f} per 100\n",
      100.0 * s.rate,
      p::BachelierSwaptionPrice(OptionType::kPut, s.rate, 0.0, 0.005, 2.0,
                                s.annuity, 100.0));
}

void Hedging(const ZeroCurve& curve) {
  const p::CurvePricer cap = [](const ZeroCurve& c) {
    return 100.0 * *p::CapFloorPrice(OptionType::kCall, c,
                                     {.strike = 0.04, .maturity = 5.0}, 0.2);
  };
  const auto buckets = p::BucketDv01(cap, curve);
  std::cout << "Bucketed DV01 of the cap (per 100, 1 bp):";
  for (std::size_t i = 0; i < buckets.size(); ++i) {
    std::cout << std::format(" {}y {:.5f}", curve.times()[i], buckets[i]);
  }
  double sum = 0.0;
  for (double b : buckets) sum += b;
  std::cout << std::format("; sum {:.5f}, parallel {:.5f}\n", sum,
                           p::ParallelDv01(cap, curve));
}

}  // namespace

int main() {
  try {
    const ZeroCurve curve = Upward();
    BookExamples();
    CapsAndStripping(curve);
    SwaptionGrid(curve);
    NormalModel();
    Hedging(curve);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
