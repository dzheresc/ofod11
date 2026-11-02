// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/exotics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <span>

#include "ofod/math/normal.h"
#include "ofod/math/roots.h"
#include "ofod/pricers/black_scholes.h"

namespace ofod::pricers {
namespace {

using math::BivariateNormCdf;
using math::NormCdf;

double Bsm(OptionType type, const TreeMarket& m, double spot, double strike,
           double t) {
  return BsmPrice(type, spot, strike, m.rate, m.yield, m.volatility, t);
}

double Sign(OptionType type) { return type == OptionType::kCall ? 1.0 : -1.0; }

}  // namespace

double GapOptionPrice(OptionType type, const TreeMarket& m, double strike,
                      double trigger, double t) {
  const double d1 = BsmD1(m.spot, trigger, m.rate, m.yield, m.volatility, t);
  const double d2 = d1 - m.volatility * std::sqrt(t);
  const double phi = Sign(type);
  return phi * (m.spot * std::exp(-m.yield * t) * NormCdf(phi * d1) -
                strike * std::exp(-m.rate * t) * NormCdf(phi * d2));
}

double ForwardStartPrice(OptionType type, const TreeMarket& m, double t1,
                         double t2) {
  return std::exp(-m.yield * t1) * Bsm(type, m, m.spot, m.spot, t2 - t1);
}

std::optional<double> PerpetualAmericanPrice(OptionType type,
                                             const TreeMarket& m,
                                             double strike) {
  const double s2 = m.volatility * m.volatility;
  const double w = m.rate - m.yield - 0.5 * s2;
  const double root = std::sqrt(w * w + 2.0 * s2 * m.rate);
  if (type == OptionType::kCall) {
    if (!(m.yield > 0.0)) return std::nullopt;
    const double a = (-w + root) / s2;
    const double boundary = a * strike / (a - 1.0);
    if (m.spot >= boundary) return m.spot - strike;
    return strike / (a - 1.0) * std::pow((a - 1.0) / a * m.spot / strike, a);
  }
  if (!(m.rate > 0.0)) return std::nullopt;
  const double a = (-w - root) / s2;
  const double boundary = a * strike / (a - 1.0);
  if (m.spot <= boundary) return strike - m.spot;
  return strike / (1.0 - a) * std::pow((a - 1.0) / a * m.spot / strike, a);
}

double CashOrNothingPrice(OptionType type, const TreeMarket& m, double strike,
                          double t, double cash) {
  const double d2 = BsmD2(m.spot, strike, m.rate, m.yield, m.volatility, t);
  return cash * std::exp(-m.rate * t) * NormCdf(Sign(type) * d2);
}

double AssetOrNothingPrice(OptionType type, const TreeMarket& m, double strike,
                           double t) {
  const double d1 = BsmD1(m.spot, strike, m.rate, m.yield, m.volatility, t);
  return m.spot * std::exp(-m.yield * t) * NormCdf(Sign(type) * d1);
}

std::optional<double> CompoundOptionPrice(CompoundType type,
                                          const TreeMarket& m, double k1,
                                          double t1, double k2, double t2) {
  if (!(t2 > t1 && t1 > 0.0 && k1 > 0.0 && k2 > 0.0)) return std::nullopt;
  const bool on_call =
      type == CompoundType::kCallOnCall || type == CompoundType::kPutOnCall;
  const OptionType underlying = on_call ? OptionType::kCall : OptionType::kPut;
  const double tau = t2 - t1;
  auto gap = [&](double s) { return Bsm(underlying, m, s, k2, tau) - k1; };
  // Bracket the critical spot S* where the underlying is worth k1.
  double lo = 1e-10 * k2;
  double hi = k2;
  if (on_call) {
    while (gap(hi) < 0.0 && hi < 1e12 * k2) hi *= 2.0;
  } else {
    if (gap(lo) <= 0.0) return std::nullopt;
    while (gap(hi) > 0.0 && hi < 1e12 * k2) hi *= 2.0;
  }
  const auto critical = math::Brent(gap, lo, hi, {.tolerance = 1e-12});
  if (!critical) return std::nullopt;
  const double v = m.volatility;
  const double a1 =
      (std::log(m.spot / *critical) + (m.rate - m.yield + 0.5 * v * v) * t1) /
      (v * std::sqrt(t1));
  const double a2 = a1 - v * std::sqrt(t1);
  const double b1 =
      (std::log(m.spot / k2) + (m.rate - m.yield + 0.5 * v * v) * t2) /
      (v * std::sqrt(t2));
  const double b2 = b1 - v * std::sqrt(t2);
  const double rho = std::sqrt(t1 / t2);
  const double s = m.spot * std::exp(-m.yield * t2);
  const double k = k2 * std::exp(-m.rate * t2);
  const double pay = k1 * std::exp(-m.rate * t1);
  switch (type) {
    case CompoundType::kCallOnCall:
      return s * BivariateNormCdf(a1, b1, rho) -
             k * BivariateNormCdf(a2, b2, rho) - pay * NormCdf(a2);
    case CompoundType::kPutOnCall:
      return k * BivariateNormCdf(-a2, b2, -rho) -
             s * BivariateNormCdf(-a1, b1, -rho) + pay * NormCdf(-a2);
    case CompoundType::kCallOnPut:
      return k * BivariateNormCdf(-a2, -b2, rho) -
             s * BivariateNormCdf(-a1, -b1, rho) - pay * NormCdf(-a2);
    case CompoundType::kPutOnPut:
      return s * BivariateNormCdf(a1, -b1, -rho) -
             k * BivariateNormCdf(a2, -b2, -rho) + pay * NormCdf(a2);
  }
  return std::nullopt;
}

double ChooserPrice(const TreeMarket& m, double strike, double t1, double t2) {
  const double tau = t2 - t1;
  return Bsm(OptionType::kCall, m, m.spot, strike, t2) +
         std::exp(-m.yield * tau) *
             Bsm(OptionType::kPut, m, m.spot,
                 strike * std::exp(-(m.rate - m.yield) * tau), t1);
}

double BarrierOptionPrice(BarrierType barrier, OptionType type,
                          const TreeMarket& m, double strike, double level,
                          double t) {
  const bool down =
      barrier == BarrierType::kDownAndOut || barrier == BarrierType::kDownAndIn;
  const bool knock_in =
      barrier == BarrierType::kDownAndIn || barrier == BarrierType::kUpAndIn;
  const double vanilla = Bsm(type, m, m.spot, strike, t);
  if (down ? m.spot <= level : m.spot >= level) {
    return knock_in ? vanilla : 0.0;
  }
  const double s = m.spot;
  const double v = m.volatility;
  const double vt = v * std::sqrt(t);
  const double mu = (m.rate - m.yield - 0.5 * v * v) / (v * v);
  const double phi = Sign(type);
  const double eta = down ? 1.0 : -1.0;
  const double carry = s * std::exp(-m.yield * t);
  const double disc = strike * std::exp(-m.rate * t);
  const double x1 = std::log(s / strike) / vt + (1.0 + mu) * vt;
  const double x2 = std::log(s / level) / vt + (1.0 + mu) * vt;
  const double y1 =
      std::log(level * level / (s * strike)) / vt + (1.0 + mu) * vt;
  const double y2 = std::log(level / s) / vt + (1.0 + mu) * vt;
  const double hs = level / s;
  const double a =
      phi * carry * NormCdf(phi * x1) - phi * disc * NormCdf(phi * (x1 - vt));
  const double b =
      phi * carry * NormCdf(phi * x2) - phi * disc * NormCdf(phi * (x2 - vt));
  const double c =
      phi * carry * std::pow(hs, 2.0 * (mu + 1.0)) * NormCdf(eta * y1) -
      phi * disc * std::pow(hs, 2.0 * mu) * NormCdf(eta * (y1 - vt));
  const double d =
      phi * carry * std::pow(hs, 2.0 * (mu + 1.0)) * NormCdf(eta * y2) -
      phi * disc * std::pow(hs, 2.0 * mu) * NormCdf(eta * (y2 - vt));
  const bool above = strike > level;
  const bool call = type == OptionType::kCall;
  double in = 0.0;
  if (down && call) {
    in = above ? c : a - b + d;
  } else if (!down && call) {
    in = above ? a : b - c + d;
  } else if (down) {
    in = above ? b - c + d : a;
  } else {
    in = above ? a - b + d : c;
  }
  return knock_in ? in : vanilla - in;
}

std::optional<double> FloatingLookbackPrice(OptionType type,
                                            const TreeMarket& m, double extreme,
                                            double t) {
  const double b = m.rate - m.yield;
  if (b == 0.0) return std::nullopt;
  const double v = m.volatility;
  const double vt = v * std::sqrt(t);
  const double ratio = v * v / (2.0 * b);
  const double s = m.spot * std::exp(-m.yield * t);
  const double disc = extreme * std::exp(-m.rate * t);
  if (type == OptionType::kCall) {
    const double l = std::log(m.spot / extreme);
    const double a1 = (l + (b + 0.5 * v * v) * t) / vt;
    const double a2 = a1 - vt;
    const double a3 = (l + (-b + 0.5 * v * v) * t) / vt;
    const double y1 = -2.0 * (b - 0.5 * v * v) * l / (v * v);
    return s * NormCdf(a1) - s * ratio * NormCdf(-a1) -
           disc * (NormCdf(a2) - ratio * std::exp(y1) * NormCdf(-a3));
  }
  const double l = std::log(extreme / m.spot);
  const double b1 = (l + (-b + 0.5 * v * v) * t) / vt;
  const double b2 = b1 - vt;
  const double b3 = (l + (b - 0.5 * v * v) * t) / vt;
  const double y2 = 2.0 * (b - 0.5 * v * v) * l / (v * v);
  return disc * (NormCdf(b1) - ratio * std::exp(y2) * NormCdf(-b3)) +
         s * ratio * NormCdf(-b2) - s * NormCdf(b2);
}

std::optional<double> FixedLookbackPrice(OptionType type, const TreeMarket& m,
                                         double strike, double extreme,
                                         double t) {
  const double s = m.spot * std::exp(-m.yield * t);
  const double k = strike * std::exp(-m.rate * t);
  if (type == OptionType::kCall) {
    const auto put = FloatingLookbackPrice(OptionType::kPut, m,
                                           std::max(extreme, strike), t);
    if (!put) return std::nullopt;
    return *put + s - k;
  }
  const auto call =
      FloatingLookbackPrice(OptionType::kCall, m, std::min(extreme, strike), t);
  if (!call) return std::nullopt;
  return *call + k - s;
}

double GeometricAsianPrice(OptionType type, const TreeMarket& m, double strike,
                           double t) {
  const double v = m.volatility;
  const double yield = 0.5 * (m.rate + m.yield + v * v / 6.0);
  return BsmPrice(type, m.spot, strike, m.rate, yield, v / std::numbers::sqrt3,
                  t);
}

std::optional<double> ArithmeticAsianPrice(OptionType type, const TreeMarket& m,
                                           double strike, double t) {
  const double b = m.rate - m.yield;
  if (b == 0.0) return std::nullopt;
  const double s2 = m.volatility * m.volatility;
  const double s0 = m.spot;
  const double m1 = (std::exp(b * t) - 1.0) / (b * t) * s0;
  const double m2 = 2.0 * std::exp((2.0 * b + s2) * t) * s0 * s0 /
                        ((b + s2) * (2.0 * b + s2) * t * t) +
                    2.0 * s0 * s0 / (b * t * t) *
                        (1.0 / (2.0 * b + s2) - std::exp(b * t) / (b + s2));
  const double vol = std::sqrt(std::log(m2 / (m1 * m1)) / t);
  return Black76Price(type, m1, strike, m.rate, vol, t);
}

double ExchangeOptionPrice(const ExchangeAssets& a, double t) {
  const double var = a.receive_vol * a.receive_vol +
                     a.deliver_vol * a.deliver_vol -
                     2.0 * a.correlation * a.receive_vol * a.deliver_vol;
  const double vt = std::sqrt(var * t);
  const double d1 = (std::log(a.receive_spot / a.deliver_spot) +
                     (a.deliver_yield - a.receive_yield + 0.5 * var) * t) /
                    vt;
  const double d2 = d1 - vt;
  return a.receive_spot * std::exp(-a.receive_yield * t) * NormCdf(d1) -
         a.deliver_spot * std::exp(-a.deliver_yield * t) * NormCdf(d2);
}

std::optional<double> VarianceSwapFairVariance(
    const TreeMarket& m, double t, std::span<const double> strikes,
    const VolatilityFunction& implied_vol) {
  const std::size_t n = strikes.size();
  if (n < 3 || !(t > 0.0)) return std::nullopt;
  for (std::size_t i = 1; i < n; ++i) {
    if (!(strikes[i] > strikes[i - 1]) || !(strikes[0] > 0.0)) {
      return std::nullopt;
    }
  }
  const double forward = m.spot * std::exp((m.rate - m.yield) * t);
  if (forward < strikes[0] || forward > strikes[n - 1]) return std::nullopt;
  std::size_t star = 0;
  while (star + 1 < n && strikes[star + 1] <= forward) ++star;
  const double s_star = strikes[star];
  double sum = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double k = strikes[i];
    const double lo = i == 0 ? k : strikes[i - 1];
    const double hi = i + 1 == n ? k : strikes[i + 1];
    const double dk = (hi - lo) / (i == 0 || i + 1 == n ? 1.0 : 2.0);
    const double vol = implied_vol(k);
    auto price = [&](OptionType type) {
      return BsmPrice(type, m.spot, k, m.rate, m.yield, vol, t);
    };
    double q = 0.0;
    if (i < star) {
      q = price(OptionType::kPut);
    } else if (i > star) {
      q = price(OptionType::kCall);
    } else {
      q = 0.5 * (price(OptionType::kPut) + price(OptionType::kCall));
    }
    sum += dk / (k * k) * std::exp(m.rate * t) * q;
  }
  return 2.0 / t * std::log(forward / s_star) -
         2.0 / t * (forward / s_star - 1.0) + 2.0 / t * sum;
}

}  // namespace ofod::pricers
