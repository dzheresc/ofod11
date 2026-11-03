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

// sqrt(z) on the principal branch. std::sqrt calls glibc's csqrt, which
// goes through hypot and edge-case handling that cannot matter here.
Complex Sqrt(Complex z) {
  const double r = std::sqrt(z.real() * z.real() + z.imag() * z.imag());
  const double a = std::sqrt(0.5 * (r + std::abs(z.real())));
  if (a == 0.0) return {0.0, 0.0};
  const double b = 0.5 * z.imag() / a;
  return z.real() >= 0.0 ? Complex(a, b)
                         : Complex(std::abs(b), std::copysign(a, z.imag()));
}

// Heston's characteristic function of ln(S_T / F), F the forward, in the
// "little trap" form: E[exp(i xi ln(S_T / F))] = exp(C(xi) + D(xi) v0).
// Returns C + D v0; the caller takes the exponential.
class HestonExponent {
 public:
  HestonExponent(double t, const HestonParams& p)
      : t_(t),
        v0_(p.v0),
        kappa_(p.kappa),
        s2_(p.sigma * p.sigma),
        rho_sigma_(p.rho * p.sigma),
        kt_over_s2_(p.kappa * p.theta / s2_) {}

  Complex operator()(Complex xi) const {
    const Complex i_xi(-xi.imag(), xi.real());  // i xi
    const Complex beta = kappa_ - rho_sigma_ * i_xi;
    const Complex d = Sqrt(beta * beta + s2_ * (i_xi + xi * xi));
    const Complex bm = beta - d;
    const Complex g = Div(bm, beta + d);
    const Complex e = std::exp(-d * t_);
    const Complex ge = 1.0 - g * e;
    const Complex c = kt_over_s2_ * (bm * t_ - 2.0 * Log(Div(ge, 1.0 - g)));
    const Complex dd = bm / s2_ * Div(1.0 - e, ge);
    return c + dd * v0_;
  }

 private:
  double t_;
  double v0_;
  double kappa_;
  double s2_;
  double rho_sigma_;
  double kt_over_s2_;
};

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
  static const std::optional<math::Quadrature> rule =
      math::GaussLegendre(16, 0.0, 1.0);
  if (!rule) return std::numeric_limits<double>::quiet_NaN();
  const math::Quadrature& panel = *rule;
  const double forward = m.spot * std::exp((m.rate - m.yield) * t);
  const double x = std::log(forward / strike);
  const HestonExponent exponent(t, params);

  // Black-Scholes control variate with the expected integrated variance
  // w = E[int_0^T v dt]. The identity holds for any w > 0; this choice
  // makes the difference of the two integrands small near u = 0, and it
  // cancels the poles of 1 / (u^2 + 1/4) at u = +-i/2 (both characteristic
  // functions are 1 there), so wide panels converge quickly.
  const double kt = params.kappa * t;
  const double mean_reversion =
      std::abs(kt) < 1e-8 ? t : -std::expm1(-kt) / params.kappa;
  const double w = std::max(
      params.theta * t + (params.v0 - params.theta) * mean_reversion, 1e-8);

  // Lewis (2001): C = BS(w) - sqrt(F K) e^{-rT} / pi
  //   int_0^inf Re[e^{iux} (psi_H(u - i/2) - psi_BS(u - i/2))] /
  //   (u^2 + 1/4) du, with psi_BS(u - i/2) = e^{-w (u^2 + 1/4) / 2}.
  auto integrand = [&](double u) {
    const Complex z = exponent(Complex(u, -0.5));
    double value = std::exp(z.real()) * std::cos(z.imag() + u * x);
    const double bs = -0.5 * w * (u * u + 0.25);
    if (bs > -60.0) value -= std::exp(bs) * std::cos(u * x);
    return value / (u * u + 0.25);
  };

  // Panel width from the integrand's length scales. Near zero it behaves
  // like e^{-w u^2 / 2}; in the tail |psi_H| decays like e^{-c u} with
  // c = sqrt(1 - rho^2) (v0 + kappa theta T) / sigma and turns with phase
  // rate rho (v0 + kappa theta T) / sigma; e^{iux} adds the moneyness. The
  // tail rates are capped at 8 sqrt(w), where the Gaussian part has long
  // decayed, so that a tiny sigma does not force tiny panels.
  const double tail = (params.v0 + kt * params.theta) / params.sigma;
  const double cap = 8.0 * std::sqrt(w);
  const double decay = std::min(
      tail * std::sqrt(std::max(1.0 - params.rho * params.rho, 0.0)), cap);
  const double phase = std::abs(x) + std::min(std::abs(params.rho) * tail, cap);
  const double scale = std::max({decay, std::sqrt(w), phase, 1e-3});
  const double width = std::min(4.0 / scale, 50.0);

  // Panels start at width 1/2 and double up to `width`: the characteristic
  // function has poles on the imaginary axis at |u| >= 1/2 (moment
  // explosions), and doubling keeps each panel as far from them, relative
  // to its width, as the first one.
  double integral = 0.0;
  double left = 0.0;
  double h = std::min(0.5, width);
  int quiet = 0;
  constexpr int kMaxPanels = 20000;
  for (int k = 0; k < kMaxPanels && quiet < 3; ++k) {
    double part = 0.0;
    for (std::size_t j = 0; j < panel.nodes.size(); ++j) {
      part += panel.weights[j] * integrand(left + h * panel.nodes[j]);
    }
    part *= h;
    left += h;
    h = std::min(2.0 * h, width);
    integral += part;
    quiet = std::abs(part) < 1e-14 * (1.0 + std::abs(integral)) ? quiet + 1 : 0;
  }
  const double call = BsmPrice(OptionType::kCall, m.spot, strike, m.rate,
                               m.yield, std::sqrt(w / t), t) -
                      std::sqrt(forward * strike) * std::exp(-m.rate * t) /
                          std::numbers::pi * integral;
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
