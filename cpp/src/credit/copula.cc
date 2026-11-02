// Copyright 2026 Denis Zhereshchin

#include "ofod/credit/copula.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ofod/credit/credit_risk.h"
#include "ofod/credit/tranche.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/math/normal.h"
#include "ofod/math/quadrature.h"
#include "ofod/math/roots.h"
#include "ofod/random/normal_generator.h"

namespace ofod::credit {
namespace {

bool ValidPool(const CdoPool& pool) {
  return pool.names >= 1 && pool.hazard >= 0.0 && pool.recovery >= 0.0 &&
         pool.recovery < 1.0;
}

bool ValidTranche(const Tranche& t) {
  return t.attachment >= 0.0 && t.detachment > t.attachment &&
         t.detachment <= 1.0;
}

}  // namespace

std::vector<double> DefaultCountDistribution(std::span<const double> p) {
  std::vector<double> dist = {1.0};
  dist.reserve(p.size() + 1);
  for (double q : p) {
    dist.push_back(0.0);
    for (std::size_t k = dist.size() - 1; k > 0; --k) {
      dist[k] = dist[k] * (1.0 - q) + dist[k - 1] * q;
    }
    dist[0] *= 1.0 - q;
  }
  return dist;
}

std::optional<double> ExpectedTrancheLoss(const Tranche& tranche,
                                          const CdoPool& pool,
                                          double correlation, double t,
                                          int quadrature_points) {
  if (!ValidPool(pool) || !ValidTranche(tranche) || !(correlation >= 0.0) ||
      !(correlation < 1.0)) {
    return std::nullopt;
  }
  const auto rule = math::GaussHermiteNormal(quadrature_points);
  if (!rule) return std::nullopt;
  if (t <= 0.0) return 0.0;
  const double q = 1.0 - std::exp(-pool.hazard * t);
  const auto names = static_cast<std::size_t>(pool.names);
  double expected = 0.0;
  for (std::size_t i = 0; i < rule->nodes.size(); ++i) {
    const double p = q <= 0.0 ? 0.0
                              : ConditionalDefaultProbability(q, correlation,
                                                              rule->nodes[i]);
    const std::vector<double> probs(names, p);
    const std::vector<double> dist = DefaultCountDistribution(probs);
    double loss = 0.0;
    for (std::size_t k = 0; k < dist.size(); ++k) {
      const double pool_loss =
          (1.0 - pool.recovery) * static_cast<double>(k) / pool.names;
      loss += dist[k] * TrancheLoss(tranche, pool_loss);
    }
    expected += rule->weights[i] * loss;
  }
  return expected;
}

std::optional<TrancheLegs> TrancheLegValues(const Tranche& tranche,
                                            const CdoPool& pool,
                                            double correlation,
                                            const CdoTerms& terms,
                                            const curves::ZeroCurve& discount) {
  if (!(terms.maturity > 0.0) || terms.frequency < 1) return std::nullopt;
  const double dt = 1.0 / terms.frequency;
  const auto periods =
      static_cast<int>(std::ceil(terms.maturity * terms.frequency - 1e-9));
  double annuity = 0.0;
  double protection = 0.0;
  double start = 0.0;
  double previous_loss = 0.0;
  for (int i = 1; i <= periods; ++i) {
    const double end = std::min(i * dt, terms.maturity);
    const auto loss = ExpectedTrancheLoss(tranche, pool, correlation, end,
                                          terms.quadrature_points);
    if (!loss) return std::nullopt;
    const double length = end - start;
    const double df_mid = discount.DiscountFactor(0.5 * (start + end));
    annuity += length * (1.0 - *loss) * discount.DiscountFactor(end) +
               0.5 * length * (*loss - previous_loss) * df_mid;
    protection += (*loss - previous_loss) * df_mid;
    previous_loss = *loss;
    start = end;
  }
  return TrancheLegs{.premium_annuity = annuity, .protection = protection};
}

std::optional<double> TrancheSpread(const Tranche& tranche, const CdoPool& pool,
                                    double correlation, const CdoTerms& terms,
                                    const curves::ZeroCurve& discount) {
  const auto legs =
      TrancheLegValues(tranche, pool, correlation, terms, discount);
  if (!legs) return std::nullopt;
  return legs->protection / legs->premium_annuity;
}

std::optional<double> ImpliedTrancheCorrelation(
    const Tranche& tranche, const CdoPool& pool, double spread,
    const CdoTerms& terms, const curves::ZeroCurve& discount) {
  auto gap = [&](double rho) {
    return TrancheSpread(tranche, pool, rho, terms, discount).value_or(0.0) -
           spread;
  };
  // Mezzanine spreads are not monotone in correlation, so scan for the
  // first sign change before polishing with Brent.
  constexpr int kGrid = 40;
  constexpr double kLo = 1e-4;
  constexpr double kHi = 0.999;
  double lo = kLo;
  double f_lo = gap(lo);
  for (int i = 1; i <= kGrid; ++i) {
    const double hi = kLo + (kHi - kLo) * i / kGrid;
    const double f_hi = gap(hi);
    if ((f_lo <= 0.0) != (f_hi <= 0.0)) {
      return math::Brent(gap, lo, hi, {.tolerance = 1e-10});
    }
    lo = hi;
    f_lo = f_hi;
  }
  return std::nullopt;
}

std::optional<BasketResult> NthToDefaultSpread(const BasketTerms& terms,
                                               double rate, int paths,
                                               std::uint64_t seed) {
  if (terms.names < 1 || terms.nth < 1 || terms.nth > terms.names ||
      !(terms.hazard > 0.0) || !(terms.correlation >= 0.0) ||
      !(terms.correlation < 1.0) || !(terms.maturity > 0.0) ||
      terms.frequency < 1 || paths < 2) {
    return std::nullopt;
  }
  random::NormalGenerator rng(seed);
  const double a = std::sqrt(terms.correlation);
  const double b = std::sqrt(1.0 - terms.correlation);
  const double dt = 1.0 / terms.frequency;
  const auto names = static_cast<std::size_t>(terms.names);
  std::vector<double> tau(names);
  double sum_protection = 0.0;
  double sum_protection_sq = 0.0;
  double sum_annuity = 0.0;
  for (int p = 0; p < paths; ++p) {
    const double f = rng.Next();
    for (double& x : tau) {
      const double u = math::NormCdf(a * f + b * rng.Next());
      x = -std::log1p(-u) / terms.hazard;
    }
    const auto nth = static_cast<std::ptrdiff_t>(terms.nth - 1);
    std::nth_element(tau.begin(), tau.begin() + nth, tau.end());
    const double stop =
        std::min(tau[static_cast<std::size_t>(nth)], terms.maturity);
    double annuity = 0.0;
    double paid_to = 0.0;
    for (double t = dt; t <= stop + 1e-12; t += dt) {
      annuity += dt * std::exp(-rate * t);
      paid_to = t;
    }
    double protection = 0.0;
    if (tau[static_cast<std::size_t>(nth)] < terms.maturity) {
      annuity += (stop - paid_to) * std::exp(-rate * stop);
      protection = (1.0 - terms.recovery) * std::exp(-rate * stop);
    }
    sum_protection += protection;
    sum_protection_sq += protection * protection;
    sum_annuity += annuity;
  }
  const double n = paths;
  const double mean_prot = sum_protection / n;
  const double var_prot =
      std::max(sum_protection_sq / n - mean_prot * mean_prot, 0.0) * n /
      (n - 1.0);
  const double mean_annuity = sum_annuity / n;
  return BasketResult{.spread = mean_prot / mean_annuity,
                      .std_error = std::sqrt(var_prot / n) / mean_annuity};
}

}  // namespace ofod::credit
