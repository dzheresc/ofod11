// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/american_mc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "ofod/math/linalg.h"
#include "ofod/random/normal_generator.h"

namespace ofod::pricers {
namespace {

double Intrinsic(OptionType type, double s, double k) {
  return type == OptionType::kCall ? std::max(s - k, 0.0)
                                   : std::max(k - s, 0.0);
}

}  // namespace

std::optional<McResult> LongstaffSchwartz(OptionType type, const TreeMarket& m,
                                          double strike, double t,
                                          const LsmOptions& options) {
  const int dates = options.exercise_dates;
  const int deg = options.basis_degree;
  if (dates < 1 || options.paths < 2 || deg < 1 || deg > 6 || !(t > 0.0) ||
      !(strike > 0.0)) {
    return std::nullopt;
  }
  const auto n = static_cast<std::size_t>(options.paths);
  const auto steps = static_cast<std::size_t>(dates);
  const double dt = t / dates;
  const double drift =
      (m.rate - m.yield - 0.5 * m.volatility * m.volatility) * dt;
  const double diffusion = m.volatility * std::sqrt(dt);
  const double df = std::exp(-m.rate * dt);

  // spot[j * n + p] is path p at date j + 1, generated date by date.
  std::vector<double> spot(n * steps);
  random::NormalGenerator rng(options.seed);
  for (std::size_t p = 0; p < n; ++p) spot[p] = m.spot;
  for (std::size_t j = 0; j < steps; ++j) {
    const double* prev = j == 0 ? nullptr : &spot[(j - 1) * n];
    double* cur = &spot[j * n];
    for (std::size_t p = 0; p < n; ++p) {
      const double s0 = prev == nullptr ? m.spot : prev[p];
      cur[p] = s0 * std::exp(drift + diffusion * rng.Next());
    }
  }

  // cash[p]: the path's cash flow discounted to the current date.
  std::vector<double> cash(n);
  const double* last = &spot[(steps - 1) * n];
  for (std::size_t p = 0; p < n; ++p) {
    cash[p] = Intrinsic(type, last[p], strike);
  }
  const auto dim = static_cast<std::size_t>(deg) + 1;
  math::Matrix normal(dim, std::vector<double>(dim));
  std::vector<double> rhs(dim);
  // The normal matrix is Hankel: entry (r, c) is sum x^(r + c), so only
  // the 2 deg + 1 power sums are accumulated per path.
  std::vector<double> power_sums(2 * dim - 1);
  for (std::size_t j = steps - 1; j-- > 0;) {
    const double* s = &spot[j * n];
    std::fill(power_sums.begin(), power_sums.end(), 0.0);
    std::fill(rhs.begin(), rhs.end(), 0.0);
    int itm = 0;
    for (std::size_t p = 0; p < n; ++p) {
      cash[p] *= df;
      if (Intrinsic(type, s[p], strike) <= 0.0) continue;
      ++itm;
      const double x = s[p] / strike;
      double power = 1.0;
      for (std::size_t k = 0; k < power_sums.size(); ++k) {
        power_sums[k] += power;
        if (k < dim) rhs[k] += power * cash[p];
        power *= x;
      }
    }
    if (itm <= deg) continue;
    for (std::size_t r = 0; r < dim; ++r) {
      for (std::size_t c = 0; c < dim; ++c) normal[r][c] = power_sums[r + c];
    }
    const auto beta = math::SolveLinear(normal, rhs);
    if (!beta) continue;
    for (std::size_t p = 0; p < n; ++p) {
      const double exercise = Intrinsic(type, s[p], strike);
      if (exercise <= 0.0) continue;
      const double x = s[p] / strike;
      double continuation = 0.0;
      for (std::size_t k = dim; k-- > 0;) {
        continuation = continuation * x + (*beta)[k];
      }
      if (exercise > continuation) cash[p] = exercise;
    }
  }
  double sum = 0.0;
  double sum_sq = 0.0;
  for (double& c : cash) {
    c *= df;
    sum += c;
    sum_sq += c * c;
  }
  const auto count = static_cast<double>(n);
  const double mean = sum / count;
  const double var =
      std::max(sum_sq / count - mean * mean, 0.0) * count / (count - 1.0);
  return McResult{.price = std::max(mean, Intrinsic(type, m.spot, strike)),
                  .std_error = std::sqrt(var / count),
                  .paths = options.paths};
}

std::optional<double> ConvertibleBondPrice(const TreeMarket& m,
                                           const ConvertibleTerms& terms,
                                           int steps) {
  if (steps < 1 || !(terms.maturity > 0.0)) return std::nullopt;
  const double dt = terms.maturity / steps;
  const double var = (m.volatility * m.volatility - terms.hazard) * dt;
  if (!(var > 0.0)) return std::nullopt;
  const double u = std::exp(std::sqrt(var));
  const double d = 1.0 / u;
  const double a = std::exp((m.rate - m.yield) * dt);
  const double survive = std::exp(-terms.hazard * dt);
  const double pu = (a - d * survive) / (u - d);
  const double pd = (u * survive - a) / (u - d);
  const double p_default = 1.0 - survive;
  if (pu < 0.0 || pd < 0.0) return std::nullopt;
  const double disc = std::exp(-m.rate * dt);
  const double recovered = terms.recovery * terms.face;
  const auto n = static_cast<std::size_t>(steps);
  std::vector<double> up_power(n + 1);
  for (std::size_t j = 0; j <= n; ++j) {
    up_power[j] = std::pow(u, static_cast<double>(j));
  }
  auto spot_at = [&](std::size_t i, std::size_t j) {
    // j up moves out of i: S u^j d^(i-j) = S u^(2j - i).
    return m.spot * up_power[j] / up_power[i - j];
  };
  std::vector<double> value(n + 1);
  for (std::size_t j = 0; j <= n; ++j) {
    value[j] = std::max(terms.face, terms.conversion_ratio * spot_at(n, j));
  }
  for (std::size_t i = n; i-- > 0;) {
    for (std::size_t j = 0; j <= i; ++j) {
      const double hold =
          disc * (pu * value[j + 1] + pd * value[j] + p_default * recovered);
      const double conversion = terms.conversion_ratio * spot_at(i, j);
      value[j] = std::max(conversion, std::min(hold, terms.call_price));
    }
  }
  return value[0];
}

}  // namespace ofod::pricers
