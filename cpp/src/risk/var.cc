// Copyright 2026 Denis Zhereshchin

#include "ofod/risk/var.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "ofod/math/linalg.h"
#include "ofod/math/normal.h"
#include "ofod/random/normal_generator.h"

namespace ofod::risk {
namespace {

// Number of tail scenarios k = round(n (1 - X)).
int TailCount(std::size_t n, double confidence) {
  return static_cast<int>(
      std::lround(static_cast<double>(n) * (1.0 - confidence)));
}

// The k worst losses, worst first. Only the tail is ordered: a partial sort
// is O(n log k) where a full sort was O(n log n), and the first k elements
// are the same values in the same order.
std::vector<double> WorstFirst(std::span<const double> losses, int k) {
  std::vector<double> sorted(losses.begin(), losses.end());
  const auto tail = static_cast<std::ptrdiff_t>(
      std::min(static_cast<std::size_t>(k), sorted.size()));
  std::partial_sort(sorted.begin(), sorted.begin() + tail, sorted.end(),
                    std::greater<>());
  return sorted;
}

}  // namespace

double NormalVar(double sd, double confidence, double mean) {
  return mean + sd * math::NormInv(confidence);
}

double NormalExpectedShortfall(double sd, double confidence, double mean) {
  return mean +
         sd * math::NormPdf(math::NormInv(confidence)) / (1.0 - confidence);
}

double ScaleToHorizon(double one_day, double days) {
  return one_day * std::sqrt(days);
}

std::optional<double> HistoricalVar(std::span<const double> losses,
                                    double confidence) {
  const int k = TailCount(losses.size(), confidence);
  if (k < 1) return std::nullopt;
  return WorstFirst(losses, k)[static_cast<std::size_t>(k) - 1];
}

std::optional<double> HistoricalExpectedShortfall(
    std::span<const double> losses, double confidence) {
  const int k = TailCount(losses.size(), confidence);
  if (k < 1) return std::nullopt;
  const std::vector<double> sorted = WorstFirst(losses, k);
  double sum = 0.0;
  for (int i = 0; i < k; ++i) sum += sorted[static_cast<std::size_t>(i)];
  return sum / k;
}

std::optional<std::vector<double>> VolatilityAdjustedChanges(
    std::span<const double> changes, std::span<const double> vols,
    double current_vol) {
  if (changes.size() != vols.size()) return std::nullopt;
  std::vector<double> out;
  out.reserve(changes.size());
  for (std::size_t i = 0; i < changes.size(); ++i) {
    if (!(vols[i] > 0.0)) return std::nullopt;
    out.push_back(changes[i] * current_vol / vols[i]);
  }
  return out;
}

std::optional<double> PortfolioStdDev(std::span<const double> exposures,
                                      std::span<const double> vols,
                                      const math::Matrix& correlation) {
  const std::size_t n = exposures.size();
  if (vols.size() != n || correlation.size() != n) return std::nullopt;
  double var = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    if (correlation[i].size() != n) return std::nullopt;
    for (std::size_t j = 0; j < n; ++j) {
      var +=
          exposures[i] * exposures[j] * vols[i] * vols[j] * correlation[i][j];
    }
  }
  return std::sqrt(std::max(var, 0.0));
}

Moments DeltaGammaMoments(double delta, double gamma, double sd_change) {
  const double s2 = sd_change * sd_change;
  const double mean = 0.5 * gamma * s2;
  const double e2 = delta * delta * s2 + 0.75 * gamma * gamma * s2 * s2;
  const double e3 = 4.5 * delta * delta * gamma * s2 * s2 +
                    1.875 * gamma * gamma * gamma * s2 * s2 * s2;
  const double var = e2 - mean * mean;
  const double sd = std::sqrt(var);
  const double third = e3 - 3.0 * e2 * mean + 2.0 * mean * mean * mean;
  return {.mean = mean, .std_dev = sd, .skewness = third / (sd * sd * sd)};
}

double CornishFisherVar(const Moments& pnl, double confidence) {
  const double z = math::NormInv(1.0 - confidence);
  const double w = z + (z * z - 1.0) * pnl.skewness / 6.0;
  return -(pnl.mean + w * pnl.std_dev);
}

std::optional<VarEstimate> MonteCarloVar(const PnlFunction& pnl,
                                         const math::Matrix& covariance,
                                         double confidence, int paths,
                                         std::uint64_t seed) {
  if (paths < 1 || TailCount(static_cast<std::size_t>(paths), confidence) < 1) {
    return std::nullopt;
  }
  const auto lower = random::Cholesky(covariance);
  if (!lower) return std::nullopt;
  random::NormalGenerator rng(seed);
  std::vector<double> losses;
  losses.reserve(static_cast<std::size_t>(paths));
  const std::size_t dims = covariance.size();
  std::vector<double> z(dims);
  for (int p = 0; p < paths; ++p) {
    for (double& x : z) x = rng.Next();
    const std::vector<double> dx = random::Correlate(*lower, z);
    losses.push_back(-pnl(dx));
  }
  const auto var = HistoricalVar(losses, confidence);
  const auto es = HistoricalExpectedShortfall(losses, confidence);
  if (!var || !es) return std::nullopt;
  return VarEstimate{.var = *var, .expected_shortfall = *es};
}

double KupiecStatistic(int observations, int exceptions, double p) {
  const double n = observations;
  const double m = exceptions;
  auto xlogy = [](double x, double y) {
    return x == 0.0 ? 0.0 : x * std::log(y);
  };
  const double q = m / n;
  const double null_ll = xlogy(n - m, 1.0 - p) + xlogy(m, p);
  const double alt_ll = xlogy(n - m, 1.0 - q) + xlogy(m, q);
  return -2.0 * (null_ll - alt_ll);
}

double BinomialTailProbability(int n, int m, double p) {
  if (m <= 0) return 1.0;
  if (m > n) return 0.0;
  double below = 0.0;
  for (int k = 0; k < m; ++k) {
    const double log_term = std::lgamma(n + 1.0) - std::lgamma(k + 1.0) -
                            std::lgamma(n - k + 1.0) + k * std::log(p) +
                            (n - k) * std::log1p(-p);
    below += std::exp(log_term);
  }
  return std::max(0.0, 1.0 - below);
}

std::optional<PcaResult> PrincipalComponents(const math::Matrix& observations) {
  const std::size_t rows = observations.size();
  if (rows < 2) return std::nullopt;
  const std::size_t cols = observations[0].size();
  if (cols == 0) return std::nullopt;
  std::vector<double> mean(cols, 0.0);
  for (const auto& row : observations) {
    if (row.size() != cols) return std::nullopt;
    for (std::size_t j = 0; j < cols; ++j) mean[j] += row[j];
  }
  for (double& m : mean) m /= static_cast<double>(rows);
  math::Matrix cov(cols, std::vector<double>(cols, 0.0));
  for (const auto& row : observations) {
    for (std::size_t i = 0; i < cols; ++i) {
      for (std::size_t j = i; j < cols; ++j) {
        cov[i][j] += (row[i] - mean[i]) * (row[j] - mean[j]);
      }
    }
  }
  for (std::size_t i = 0; i < cols; ++i) {
    for (std::size_t j = i; j < cols; ++j) {
      cov[i][j] /= static_cast<double>(rows - 1);
      cov[j][i] = cov[i][j];
    }
  }
  auto eigen = math::SymmetricEigen(cov);
  if (!eigen) return std::nullopt;
  return PcaResult{.variances = std::move(eigen->values),
                   .loadings = std::move(eigen->vectors)};
}

std::optional<double> PcaStdDev(std::span<const double> exposures,
                                const math::Matrix& loadings,
                                std::span<const double> factor_sds) {
  if (loadings.size() != factor_sds.size()) return std::nullopt;
  double var = 0.0;
  for (std::size_t k = 0; k < loadings.size(); ++k) {
    if (loadings[k].size() != exposures.size()) return std::nullopt;
    double exposure = 0.0;
    for (std::size_t i = 0; i < exposures.size(); ++i) {
      exposure += exposures[i] * loadings[k][i];
    }
    var += exposure * exposure * factor_sds[k] * factor_sds[k];
  }
  return std::sqrt(var);
}

}  // namespace ofod::risk
