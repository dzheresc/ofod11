// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/rate_options.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ofod/math/normal.h"
#include "ofod/math/roots.h"

namespace ofod::pricers {
namespace {

using math::NormCdf;
using math::NormPdf;

double Sign(OptionType type) { return type == OptionType::kCall ? 1.0 : -1.0; }

curves::ZeroCurve Bumped(const curves::ZeroCurve& curve,
                         std::span<const double> bumps) {
  std::vector<double> rates(curve.zero_rates().begin(),
                            curve.zero_rates().end());
  for (std::size_t i = 0; i < rates.size(); ++i) rates[i] += bumps[i];
  std::vector<double> times(curve.times().begin(), curve.times().end());
  // The pillars come from a valid curve, so Create cannot fail.
  auto bumped = curves::ZeroCurve::Create(std::move(times), std::move(rates));
  if (!bumped) std::abort();
  return std::move(bumped).value();
}

}  // namespace

double ForwardBondPrice(double spot_price, double coupon_pv, double discount) {
  return (spot_price - coupon_pv) / discount;
}

double BlackFormula(OptionType type, double forward, double strike,
                    double volatility, double t) {
  const double phi = Sign(type);
  const double sd = volatility * std::sqrt(t);
  if (sd <= 0.0) return std::max(phi * (forward - strike), 0.0);
  const double d1 = std::log(forward / strike) / sd + 0.5 * sd;
  const double d2 = d1 - sd;
  return phi * (forward * NormCdf(phi * d1) - strike * NormCdf(phi * d2));
}

double BachelierFormula(OptionType type, double forward, double strike,
                        double normal_vol, double t) {
  const double phi = Sign(type);
  const double sd = normal_vol * std::sqrt(t);
  if (sd <= 0.0) return std::max(phi * (forward - strike), 0.0);
  const double d = (forward - strike) / sd;
  return phi * (forward - strike) * NormCdf(phi * d) + sd * NormPdf(d);
}

double BlackBondOptionPrice(OptionType type, double forward_price,
                            double strike, double discount, double volatility,
                            double t) {
  return discount * BlackFormula(type, forward_price, strike, volatility, t);
}

double PriceVolatilityFromYield(double modified_duration, double forward_yield,
                                double yield_volatility) {
  return modified_duration * forward_yield * yield_volatility;
}

double BlackCapletPrice(OptionType type, double forward, double strike,
                        double volatility, double reset, double accrual,
                        double discount, double notional) {
  return notional * accrual * discount *
         BlackFormula(type, forward, strike, volatility, reset);
}

int CapletCount(const CapTerms& terms) {
  if (terms.frequency <= 0 || terms.maturity <= 0.0) return 0;
  const double periods = terms.maturity * terms.frequency;
  const double n = std::round(periods);
  if (std::abs(periods - n) > 1e-9 || n < 2.0) return 0;
  return static_cast<int>(n) - 1;
}

std::optional<std::vector<double>> CapletPrices(
    OptionType type, const curves::ZeroCurve& curve, const CapTerms& terms,
    std::span<const double> volatilities) {
  const int n = CapletCount(terms);
  if (n == 0 || volatilities.size() != static_cast<std::size_t>(n)) {
    return std::nullopt;
  }
  const double tau = 1.0 / terms.frequency;
  std::vector<double> prices(volatilities.size());
  for (std::size_t k = 0; k < prices.size(); ++k) {
    const double reset = static_cast<double>(k + 1) * tau;
    const double pay = reset + tau;
    prices[k] = BlackCapletPrice(type, SimpleForwardRate(curve, reset, pay),
                                 terms.strike, volatilities[k], reset, tau,
                                 curve.DiscountFactor(pay), terms.notional);
  }
  return prices;
}

std::optional<double> CapFloorPrice(OptionType type,
                                    const curves::ZeroCurve& curve,
                                    const CapTerms& terms,
                                    double flat_volatility) {
  const int n = CapletCount(terms);
  if (n == 0) return std::nullopt;
  const std::vector<double> vols(static_cast<std::size_t>(n), flat_volatility);
  const auto prices = CapletPrices(type, curve, terms, vols);
  if (!prices) return std::nullopt;
  double total = 0.0;
  for (double p : *prices) total += p;
  return total;
}

std::optional<std::vector<double>> StripCapletVolatilities(
    const curves::ZeroCurve& curve, double strike, int frequency,
    std::span<const double> flat_vols, double notional) {
  if (frequency <= 0 || flat_vols.empty()) return std::nullopt;
  const double tau = 1.0 / frequency;
  std::vector<double> spot;
  spot.reserve(flat_vols.size());
  double previous_cap = 0.0;  // Value of the cap with the caplets so far.
  for (std::size_t i = 0; i < flat_vols.size(); ++i) {
    const CapTerms terms{.strike = strike,
                         .maturity = static_cast<double>(i + 2) * tau,
                         .frequency = frequency,
                         .notional = notional};
    const auto cap =
        CapFloorPrice(OptionType::kCall, curve, terms, flat_vols[i]);
    if (!cap) return std::nullopt;
    const double target = *cap - previous_cap;
    const double reset = static_cast<double>(i + 1) * tau;
    const double pay = reset + tau;
    const double forward = SimpleForwardRate(curve, reset, pay);
    const double discount = curve.DiscountFactor(pay);
    auto error = [&](double vol) {
      return BlackCapletPrice(OptionType::kCall, forward, strike, vol, reset,
                              tau, discount, notional) -
             target;
    };
    const auto vol = math::Brent(error, 1e-6, 5.0);
    if (!vol) return std::nullopt;
    spot.push_back(*vol);
    previous_cap = *cap;
  }
  return spot;
}

ForwardSwap ForwardSwapRate(const curves::ZeroCurve& curve, double start,
                            double tenor, int frequency) {
  const double tau = 1.0 / frequency;
  const int n = static_cast<int>(std::lround(tenor * frequency));
  double annuity = 0.0;
  for (int i = 1; i <= n; ++i) {
    annuity += tau * curve.DiscountFactor(start + i * tau);
  }
  const double end = start + n * tau;
  return {.annuity = annuity,
          .rate = (curve.DiscountFactor(start) - curve.DiscountFactor(end)) /
                  annuity};
}

double BlackSwaptionPrice(OptionType type, double forward_rate, double strike,
                          double volatility, double expiry, double annuity,
                          double notional) {
  return notional * annuity *
         BlackFormula(type, forward_rate, strike, volatility, expiry);
}

double BachelierSwaptionPrice(OptionType type, double forward_rate,
                              double strike, double normal_vol, double expiry,
                              double annuity, double notional) {
  return notional * annuity *
         BachelierFormula(type, forward_rate, strike, normal_vol, expiry);
}

std::vector<double> BucketDv01(const CurvePricer& value,
                               const curves::ZeroCurve& curve, double bump) {
  const double base = value(curve);
  const std::size_t n = curve.times().size();
  std::vector<double> out(n);
  std::vector<double> bumps(n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    bumps[i] = bump;
    out[i] = value(Bumped(curve, bumps)) - base;
    bumps[i] = 0.0;
  }
  return out;
}

double ParallelDv01(const CurvePricer& value, const curves::ZeroCurve& curve,
                    double bump) {
  const std::vector<double> bumps(curve.times().size(), bump);
  return value(Bumped(curve, bumps)) - value(curve);
}

}  // namespace ofod::pricers
