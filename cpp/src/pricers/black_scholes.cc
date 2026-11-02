// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/black_scholes.h"

#include <algorithm>
#include <cmath>

#include "ofod/math/normal.h"
#include "ofod/math/roots.h"

namespace ofod::pricers {

namespace {

using math::NormCdf;
using math::NormPdf;

constexpr double kMinVol = 1e-8;
constexpr double kMaxVol = 10.0;

double Vega(double spot, double strike, double rate, double yield,
            double volatility, double t) {
  const double d1 = BsmD1(spot, strike, rate, yield, volatility, t);
  return spot * std::exp(-yield * t) * NormPdf(d1) * std::sqrt(t);
}

double PresentValue(std::span<const instruments::CashFlow> flows, double rate,
                    double horizon) {
  double pv = 0.0;
  for (const auto& f : flows) {
    if (f.time <= horizon) pv += f.amount * std::exp(-rate * f.time);
  }
  return pv;
}

}  // namespace

double BsmD1(double spot, double strike, double rate, double yield,
             double volatility, double t) {
  return (std::log(spot / strike) +
          (rate - yield + 0.5 * volatility * volatility) * t) /
         (volatility * std::sqrt(t));
}

double BsmD2(double spot, double strike, double rate, double yield,
             double volatility, double t) {
  return BsmD1(spot, strike, rate, yield, volatility, t) -
         volatility * std::sqrt(t);
}

double BsmPrice(OptionType type, double spot, double strike, double rate,
                double yield, double volatility, double t) {
  const double d1 = BsmD1(spot, strike, rate, yield, volatility, t);
  const double d2 = d1 - volatility * std::sqrt(t);
  const double s = spot * std::exp(-yield * t);
  const double k = strike * std::exp(-rate * t);
  if (type == OptionType::kCall) return s * NormCdf(d1) - k * NormCdf(d2);
  return k * NormCdf(-d2) - s * NormCdf(-d1);
}

double GarmanKohlhagenPrice(OptionType type, double spot, double strike,
                            double rate, double foreign_rate, double volatility,
                            double t) {
  return BsmPrice(type, spot, strike, rate, foreign_rate, volatility, t);
}

double Black76Price(OptionType type, double futures, double strike, double rate,
                    double volatility, double t) {
  // Black's formula is BSM with q = r applied to the futures price.
  return BsmPrice(type, futures, strike, rate, rate, volatility, t);
}

double FuturesStyleOptionPrice(OptionType type, double futures, double strike,
                               double volatility, double t) {
  return Black76Price(type, futures, strike, 0.0, volatility, t);
}

double FuturesPutFromParity(double call, double futures, double strike,
                            double rate, double t) {
  return call + (strike - futures) * std::exp(-rate * t);
}

std::optional<double> ImpliedVolatility(OptionType type, double price,
                                        double spot, double strike, double rate,
                                        double yield, double t) {
  const double s = spot * std::exp(-yield * t);
  const double k = strike * std::exp(-rate * t);
  const double lower =
      type == OptionType::kCall ? std::max(s - k, 0.0) : std::max(k - s, 0.0);
  const double upper = type == OptionType::kCall ? s : k;
  if (!(price > lower && price < upper) || t <= 0.0) return std::nullopt;

  auto error = [&](double vol) {
    return BsmPrice(type, spot, strike, rate, yield, vol, t) - price;
  };
  // Newton-Raphson from the Brenner-Subrahmanyam style guess.
  double vol = std::max(std::sqrt(2.0 * std::abs(std::log(s / k)) / t), 0.2);
  for (int i = 0; i < 50; ++i) {
    const double v = Vega(spot, strike, rate, yield, vol, t);
    if (v < 1e-12) break;
    const double next = vol - error(vol) / v;
    if (!(next > kMinVol && next < kMaxVol)) break;
    if (std::abs(next - vol) < 1e-14) return next;
    vol = next;
  }
  return math::Brent(error, kMinVol, kMaxVol, {.tolerance = 1e-14});
}

double BsmPriceWithDividends(OptionType type, double spot, double strike,
                             double rate, double volatility, double t,
                             std::span<const instruments::CashFlow> dividends) {
  const double adjusted = spot - PresentValue(dividends, rate, t);
  return BsmPrice(type, adjusted, strike, rate, 0.0, volatility, t);
}

double BlackAmericanCallApproximation(
    double spot, double strike, double rate, double volatility, double t,
    std::span<const instruments::CashFlow> dividends) {
  double best = BsmPriceWithDividends(OptionType::kCall, spot, strike, rate,
                                      volatility, t, dividends);
  double last = 0.0;
  for (const auto& d : dividends) {
    if (d.time < t) last = std::max(last, d.time);
  }
  if (last > 0.0) {
    // Exercise just before the last dividend: that dividend is not received
    // by the option holder's European proxy, so exclude it.
    double pv = 0.0;
    for (const auto& d : dividends) {
      if (d.time < last) pv += d.amount * std::exp(-rate * d.time);
    }
    best = std::max(best, BsmPrice(OptionType::kCall, spot - pv, strike, rate,
                                   0.0, volatility, last));
  }
  return best;
}

double WarrantPrice(double spot, double strike, double rate, double volatility,
                    double t, double shares, double warrants) {
  return shares / (shares + warrants) *
         BsmPrice(OptionType::kCall, spot, strike, rate, 0.0, volatility, t);
}

}  // namespace ofod::pricers
