// Copyright 2026 Denis Zhereshchin

#include "ofod/models/lmm.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "ofod/random/normal_generator.h"

namespace ofod::models {
namespace {

std::size_t Size(int n) { return static_cast<std::size_t>(n); }

bool Valid(const LmmSpec& spec) {
  const std::size_t n = spec.forwards.size();
  if (!(spec.accrual > 0.0) || n == 0 || spec.loadings.size() + 1 < n) {
    return false;
  }
  if (n == 1) return true;
  const std::size_t factors = spec.loadings.front().size();
  if (factors == 0) return false;
  return std::all_of(spec.loadings.begin(), spec.loadings.end(),
                     [factors](const std::vector<double>& row) {
                       return row.size() == factors;
                     });
}

double Dot(std::span<const double> a, std::span<const double> b) {
  double sum = 0.0;
  for (std::size_t q = 0; q < a.size(); ++q) sum += a[q] * b[q];
  return sum;
}

// Loadings flattened row-major (row i = Lambda_i, `factors` wide) with the
// Ito terms |Lambda_i|^2 / 2, computed once per simulation.
struct FlatLoadings {
  std::size_t factors;
  std::vector<double> values;
  std::vector<double> half_variance;

  [[nodiscard]] std::span<const double> Row(int i) const {
    return std::span<const double>(values).subspan(Size(i) * factors, factors);
  }
};

FlatLoadings Flatten(const LmmSpec& spec, std::size_t factors) {
  FlatLoadings out{.factors = factors, .values = {}, .half_variance = {}};
  for (const std::vector<double>& row : spec.loadings) {
    out.values.insert(out.values.end(), row.begin(), row.end());
    out.half_variance.push_back(0.5 * Dot(row, row));
  }
  return out;
}

// Spot-measure drifts mu_k (per unit time) for k = j + 1, ..., n - 1 from
// forwards f (f[k] = F_k), written to mu[k].
void Drifts(const FlatLoadings& lam, double tau, int j,
            std::span<const double> f, std::span<double> sum,
            std::span<double> mu) {
  std::fill(sum.begin(), sum.end(), 0.0);
  const int n = static_cast<int>(f.size());
  for (int k = j + 1; k < n; ++k) {
    const std::span<const double> lambda = lam.Row(k - j - 1);
    const double weight = tau * f[Size(k)] / (1.0 + tau * f[Size(k)]);
    double drift = 0.0;
    for (std::size_t q = 0; q < lam.factors; ++q) {
      sum[q] += weight * lambda[q];
      drift += sum[q] * lambda[q];
    }
    mu[Size(k)] = drift - lam.half_variance[Size(k - j - 1)];
  }
}

}  // namespace

std::optional<std::vector<double>> LmmVolatilitiesFromCaplets(
    std::span<const double> caplet_vols) {
  std::vector<double> lambdas;
  double cumulative = 0.0;  // sum of Lambda^2 so far, per accrual period.
  for (std::size_t k = 0; k < caplet_vols.size(); ++k) {
    const double total =
        caplet_vols[k] * caplet_vols[k] * static_cast<double>(k + 1);
    const double next = total - cumulative;
    if (next < 0.0) return std::nullopt;
    lambdas.push_back(std::sqrt(next));
    cumulative = total;
  }
  return lambdas;
}

Matrix OneFactorLoadings(std::span<const double> lambdas) {
  Matrix out;
  out.reserve(lambdas.size());
  for (double l : lambdas) out.push_back({l});
  return out;
}

std::vector<double> LmmDiscountFactors(const LmmSpec& spec) {
  std::vector<double> p = {1.0};
  for (double f : spec.forwards) {
    p.push_back(p.back() / (1.0 + spec.accrual * f));
  }
  return p;
}

LmmPath::LmmPath(int n)
    : n_(n),
      forwards_(Size(n) * Size(n + 1) / 2),
      numeraire_(Size(n) + 1, 1.0) {}

std::size_t LmmPath::Offset(int j) const {
  const auto jj = Size(j);
  return jj * Size(n_) - jj * (jj - 1) / 2;
}

std::optional<pricers::McResult> LmmMonteCarlo(const LmmSpec& spec,
                                               const LmmPayoff& payoff,
                                               const LmmOptions& options) {
  if (!Valid(spec) || options.paths <= 0) return std::nullopt;
  const int n = static_cast<int>(spec.forwards.size());
  const std::size_t factors = n > 1 ? spec.loadings.front().size() : 0;
  const double tau = spec.accrual;
  const double sqrt_tau = std::sqrt(tau);
  const FlatLoadings lam = Flatten(spec, factors);
  random::NormalGenerator rng(options.seed);
  LmmPath path(n);
  std::vector<double> f(Size(n));
  std::vector<double> predicted(Size(n));
  std::vector<double> mu(Size(n));
  std::vector<double> mu_end(Size(n));
  std::vector<double> sum(factors);
  std::vector<double> eps(factors);
  std::vector<double> shock(Size(n));
  double total = 0.0;
  double total_sq = 0.0;
  for (int p = 0; p < options.paths; ++p) {
    std::copy(spec.forwards.begin(), spec.forwards.end(), f.begin());
    for (int k = 0; k < n; ++k) path.MutableForward(0, k) = f[Size(k)];
    path.MutableNumeraire(0) = 1.0;
    for (int j = 0; j + 1 < n; ++j) {
      for (double& e : eps) e = rng.Next();
      Drifts(lam, tau, j, f, sum, mu);
      for (int k = j + 1; k < n; ++k) {
        shock[Size(k)] = Dot(lam.Row(k - j - 1), eps) * sqrt_tau;
      }
      if (options.predictor_corrector) {
        for (int k = j + 1; k < n; ++k) {
          predicted[Size(k)] =
              f[Size(k)] * std::exp(mu[Size(k)] * tau + shock[Size(k)]);
        }
        Drifts(lam, tau, j, predicted, sum, mu_end);
        for (int k = j + 1; k < n; ++k) {
          mu[Size(k)] = 0.5 * (mu[Size(k)] + mu_end[Size(k)]);
        }
      }
      path.MutableNumeraire(j + 1) =
          path.Numeraire(j) * (1.0 + tau * f[Size(j)]);
      for (int k = j + 1; k < n; ++k) {
        f[Size(k)] *= std::exp(mu[Size(k)] * tau + shock[Size(k)]);
        path.MutableForward(j + 1, k) = f[Size(k)];
      }
    }
    path.MutableNumeraire(n) =
        path.Numeraire(n - 1) * (1.0 + tau * f[Size(n - 1)]);
    const double v = payoff(path);
    total += v;
    total_sq += v * v;
  }
  const double count = options.paths;
  const double mean = total / count;
  const double var = std::max(total_sq / count - mean * mean, 0.0);
  return pricers::McResult{.price = mean,
                           .std_error = std::sqrt(var / count),
                           .paths = options.paths};
}

std::optional<double> RebonatoSwaptionVolatility(const LmmSpec& spec, int start,
                                                 int end) {
  const int n = static_cast<int>(spec.forwards.size());
  if (!Valid(spec) || start < 1 || end > n || start >= end) {
    return std::nullopt;
  }
  const double tau = spec.accrual;
  const std::vector<double> p = LmmDiscountFactors(spec);
  double annuity = 0.0;
  for (int k = start; k < end; ++k) annuity += tau * p[Size(k + 1)];
  const double s0 = (p[Size(start)] - p[Size(end)]) / annuity;
  double variance = 0.0;
  for (int k = start; k < end; ++k) {
    const double wk = tau * p[Size(k + 1)] / annuity * spec.forwards[Size(k)];
    for (int l = start; l < end; ++l) {
      const double wl = tau * p[Size(l + 1)] / annuity * spec.forwards[Size(l)];
      double cov = 0.0;
      for (int j = 0; j < start; ++j) {
        cov += tau * Dot(spec.loadings[Size(k - j - 1)],
                         spec.loadings[Size(l - j - 1)]);
      }
      variance += wk * wl * cov;
    }
  }
  return std::sqrt(variance / (s0 * s0 * start * tau));
}

std::optional<pricers::McResult> HjmZeroBondOption(const HjmSpec& spec,
                                                   instruments::OptionType type,
                                                   int expiry, int maturity,
                                                   double strike, int paths,
                                                   std::uint64_t seed) {
  const int m = static_cast<int>(spec.forwards.size());
  if (!(spec.dt > 0.0) || !spec.volatility || expiry < 0 || maturity > m ||
      expiry > maturity || paths <= 0) {
    return std::nullopt;
  }
  const double dt = spec.dt;
  const double sqrt_dt = std::sqrt(dt);
  const double phi = type == instruments::OptionType::kCall ? 1.0 : -1.0;
  // v(t_j, T_i) does not depend on the path, so tabulate it once.
  std::vector<std::vector<double>> vol(Size(expiry));
  for (int j = 0; j < expiry; ++j) {
    for (int i = 0; i < m; ++i) {
      vol[Size(j)].push_back(spec.volatility(j * dt, i * dt));
    }
  }
  random::NormalGenerator rng(seed);
  std::vector<double> f(Size(m));
  double total = 0.0;
  double total_sq = 0.0;
  for (int p = 0; p < paths; ++p) {
    std::copy(spec.forwards.begin(), spec.forwards.end(), f.begin());
    double integral = 0.0;
    for (int j = 0; j < expiry; ++j) {
      integral += f[Size(j)] * dt;
      const double eps = rng.Next();
      double u = 0.0;
      for (int i = j + 1; i < maturity; ++i) {
        const double v = vol[Size(j)][Size(i)];
        const double u_next = u + v * dt;
        f[Size(i)] += 0.5 * (u_next * u_next - u * u) + v * sqrt_dt * eps;
        u = u_next;
      }
    }
    double log_bond = 0.0;
    for (int i = expiry; i < maturity; ++i) log_bond -= f[Size(i)] * dt;
    const double value = std::exp(-integral) *
                         std::max(phi * (std::exp(log_bond) - strike), 0.0);
    total += value;
    total_sq += value * value;
  }
  const double count = paths;
  const double mean = total / count;
  const double var = std::max(total_sq / count - mean * mean, 0.0);
  return pricers::McResult{
      .price = mean, .std_error = std::sqrt(var / count), .paths = paths};
}

}  // namespace ofod::models
