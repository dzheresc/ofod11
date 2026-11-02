// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/model_pricers.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>

#include "ofod/math/quadrature.h"
#include "ofod/math/special.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/random/normal_generator.h"

namespace ofod::pricers {
namespace {

using Complex = std::complex<double>;

double PutFromCall(double call, const TreeMarket& m, double strike, double t) {
  return call - m.spot * std::exp(-m.yield * t) +
         strike * std::exp(-m.rate * t);
}

// Principal complex logarithm. glibc's clog takes a slow exact path (it
// sorts partial products with qsort) when |z| is close to 1, which is
// exactly where Heston's (1 - g e^{-dT}) / (1 - g) lives; the plain
// formula is accurate to rounding there and 3x faster overall.
Complex Log(Complex z) { return {0.5 * std::log(std::norm(z)), std::arg(z)}; }

// a / b without libgcc's __divdc3, which guards against overflow and NaN
// that cannot occur here and cost 10% of the Heston integral.
Complex Div(Complex a, Complex b) { return a * std::conj(b) / std::norm(b); }

// Characteristic function E[exp(i u ln S_T)], u complex.
Complex HestonCf(Complex u, const TreeMarket& m, double t,
                 const HestonParams& p) {
  const Complex i(0.0, 1.0);
  const double s2 = p.sigma * p.sigma;
  const Complex beta = p.kappa - p.rho * p.sigma * i * u;
  const Complex d = std::sqrt(beta * beta + s2 * (i * u + u * u));
  const Complex g = Div(beta - d, beta + d);
  const Complex e = std::exp(-d * t);
  const Complex c = (m.rate - m.yield) * i * u * t +
                    p.kappa * p.theta / s2 *
                        ((beta - d) * t - 2.0 * Log(Div(1.0 - g * e, 1.0 - g)));
  const Complex dd = (beta - d) / s2 * Div(1.0 - e, 1.0 - g * e);
  return std::exp(c + dd * p.v0 + i * u * std::log(m.spot));
}

}  // namespace

std::optional<double> CevPrice(OptionType type, const TreeMarket& m,
                               double strike, double t, double alpha) {
  if (!(alpha > 0.0) || std::abs(alpha - 1.0) < 0.01) return std::nullopt;
  const double b_rate = m.rate - m.yield;
  const double sigma = m.volatility;
  const double v = b_rate == 0.0
                       ? sigma * sigma * t
                       : sigma * sigma / (2.0 * b_rate * (alpha - 1.0)) *
                             (std::exp(2.0 * b_rate * (alpha - 1.0) * t) - 1.0);
  const double one = 1.0 - alpha;
  const double a =
      std::pow(strike * std::exp(-b_rate * t), 2.0 * one) / (one * one * v);
  const double b = 1.0 / one;
  const double c = std::pow(m.spot, 2.0 * one) / (one * one * v);
  const double s = m.spot * std::exp(-m.yield * t);
  const double k = strike * std::exp(-m.rate * t);
  using math::NoncentralChiSquareCdf;
  double call = 0.0;
  if (alpha < 1.0) {
    call = s * (1.0 - NoncentralChiSquareCdf(a, b + 2.0, c)) -
           k * NoncentralChiSquareCdf(c, b, a);
  } else {
    call = s * (1.0 - NoncentralChiSquareCdf(c, -b, a)) -
           k * NoncentralChiSquareCdf(a, 2.0 - b, c);
  }
  return type == OptionType::kCall ? call : PutFromCall(call, m, strike, t);
}

double MertonJumpPrice(OptionType type, const TreeMarket& m, double strike,
                       double t, const JumpParams& jumps) {
  const double lambda_t = jumps.intensity * (1.0 + jumps.mean_jump) * t;
  if (!(lambda_t > 0.0)) {
    return BsmPrice(type, m.spot, strike, m.rate, m.yield, m.volatility, t);
  }
  const double gamma = std::log1p(jumps.mean_jump);
  double total = 0.0;
  constexpr int kMaxTerms = 500;
  for (int n = 0; n < kMaxTerms; ++n) {
    const double weight =
        std::exp(-lambda_t + n * std::log(lambda_t) - std::lgamma(n + 1.0));
    const double vol = std::sqrt(m.volatility * m.volatility +
                                 n * jumps.jump_vol * jumps.jump_vol / t);
    const double rate =
        m.rate - jumps.intensity * jumps.mean_jump + n * gamma / t;
    total += weight * BsmPrice(type, m.spot, strike, rate, m.yield, vol, t);
    if (n > lambda_t && weight < 1e-17) break;
  }
  return total;
}

double HestonPrice(OptionType type, const TreeMarket& m, double strike,
                   double t, const HestonParams& params) {
  const auto rule = math::GaussLegendre(16, 0.0, 1.0);
  if (!rule) return std::numeric_limits<double>::quiet_NaN();
  const math::Quadrature& panel = *rule;
  const Complex i(0.0, 1.0);
  const double log_k = std::log(strike);
  auto integrand = [&](double u) {
    const Complex num = HestonCf(Complex(u, -1.0), m, t, params) -
                        strike * HestonCf(Complex(u, 0.0), m, t, params);
    // Re[e^{-iu ln K} num / (iu)] = Im[e^{-iu ln K} num] / u.
    return (std::exp(-i * u * log_k) * num).imag() / u;
  };
  double integral = 0.0;
  int quiet = 0;
  constexpr int kMaxPanels = 2000;
  for (int k = 0; k < kMaxPanels && quiet < 3; ++k) {
    double part = 0.0;
    for (std::size_t j = 0; j < panel.nodes.size(); ++j) {
      part += panel.weights[j] * integrand(k + panel.nodes[j]);
    }
    integral += part;
    quiet = std::abs(part) < 1e-14 * (1.0 + std::abs(integral)) ? quiet + 1 : 0;
  }
  const double call =
      0.5 * (m.spot * std::exp(-m.yield * t) - strike * std::exp(-m.rate * t)) +
      std::exp(-m.rate * t) / std::numbers::pi * integral;
  return type == OptionType::kCall ? call : PutFromCall(call, m, strike, t);
}

std::optional<McResult> HestonMonteCarlo(OptionType type, const TreeMarket& m,
                                         double strike, double t,
                                         const HestonParams& params, int steps,
                                         int paths, std::uint64_t seed) {
  if (steps < 1 || paths < 2 || !(t > 0.0) || !(m.spot > 0.0)) {
    return std::nullopt;
  }
  random::NormalGenerator rng(seed);
  const double dt = t / steps;
  const double sqrt_dt = std::sqrt(dt);
  const double rho_bar = std::sqrt(1.0 - params.rho * params.rho);
  const double disc = std::exp(-m.rate * t);
  double sum = 0.0;
  double sum_sq = 0.0;
  for (int p = 0; p < paths; ++p) {
    double x = std::log(m.spot);
    double v = params.v0;
    for (int k = 0; k < steps; ++k) {
      const double z1 = rng.Next();
      const double z2 = params.rho * z1 + rho_bar * rng.Next();
      const double vp = std::max(v, 0.0);
      const double root = std::sqrt(vp) * sqrt_dt;
      x += (m.rate - m.yield - 0.5 * vp) * dt + root * z1;
      v += params.kappa * (params.theta - vp) * dt + params.sigma * root * z2;
    }
    const double s = std::exp(x);
    const double payoff =
        disc * (type == OptionType::kCall ? std::max(s - strike, 0.0)
                                          : std::max(strike - s, 0.0));
    sum += payoff;
    sum_sq += payoff * payoff;
  }
  const double n = paths;
  const double mean = sum / n;
  const double var = std::max(sum_sq / n - mean * mean, 0.0) * n / (n - 1.0);
  return McResult{
      .price = mean, .std_error = std::sqrt(var / n), .paths = paths};
}

}  // namespace ofod::pricers
