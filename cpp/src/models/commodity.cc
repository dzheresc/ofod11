// Copyright 2026 Denis Zhereshchin

#include "ofod/models/commodity.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <vector>

#include "ofod/math/linalg.h"
#include "ofod/math/quadrature.h"

namespace ofod::models {
namespace {

constexpr int kMonths = 12;

std::size_t MonthOf(double t) {
  const auto month = static_cast<std::int64_t>(std::floor(12.0 * t + 1e-9));
  const std::int64_t m = ((month % kMonths) + kMonths) % kMonths;
  return static_cast<std::size_t>(m);
}

bool ValidSpike(const SpikeModel& m) {
  return m.a > 0.0 && m.sigma >= 0.0 && m.intensity >= 0.0 && m.jump_vol >= 0.0;
}

// One exact step of x: OU decay and noise plus the step's jumps, each
// decayed from a uniform time within the step.
class SpikeStepper {
 public:
  SpikeStepper(const SpikeModel& m, double dt, std::uint64_t seed)
      : model_(m),
        dt_(dt),
        decay_(std::exp(-m.a * dt)),
        noise_(m.sigma *
               std::sqrt((1.0 - std::exp(-2.0 * m.a * dt)) / (2.0 * m.a))),
        engine_(seed),
        poisson_(m.intensity > 0.0 ? m.intensity * dt : 1.0) {}

  double Step(double x) {
    double next = x * decay_ + noise_ * normal_(engine_);
    if (model_.intensity > 0.0) {
      const int jumps = poisson_(engine_);
      for (int k = 0; k < jumps; ++k) {
        const double size =
            model_.jump_mean + model_.jump_vol * normal_(engine_);
        next += size * std::exp(-model_.a * dt_ * (1.0 - uniform_(engine_)));
      }
    }
    return next;
  }

 private:
  SpikeModel model_;
  double dt_;
  double decay_;
  double noise_;
  std::mt19937_64 engine_;
  std::normal_distribution<double> normal_;
  std::uniform_real_distribution<double> uniform_;
  std::poisson_distribution<int> poisson_;
};

}  // namespace

double SeasonalCurve::Forward(double t) const {
  return std::exp(level + growth * t + factors[MonthOf(t)]);
}

double SeasonalCurve::Multiplier(double t) const {
  return std::exp(factors[MonthOf(t)]);
}

std::optional<SeasonalCurve> FitSeasonalCurve(
    std::span<const double> maturities, std::span<const double> futures) {
  const std::size_t n = maturities.size();
  if (n != futures.size() || n < kMonths + 2) return std::nullopt;
  // Regressors: 1, t and d_m - d_11 for m = 0..10, so the factors sum to 0.
  constexpr std::size_t kParams = kMonths + 1;
  math::Matrix normal(kParams, std::vector<double>(kParams, 0.0));
  std::vector<double> rhs(kParams, 0.0);
  std::array<bool, kMonths> seen{};
  for (std::size_t i = 0; i < n; ++i) {
    if (!(futures[i] > 0.0)) return std::nullopt;
    const std::size_t month = MonthOf(maturities[i]);
    seen[month] = true;
    std::vector<double> row(kParams, 0.0);
    row[0] = 1.0;
    row[1] = maturities[i];
    for (std::size_t m = 0; m + 1 < kMonths; ++m) {
      if (month == m) row[2 + m] = 1.0;
      if (month == kMonths - 1) row[2 + m] = -1.0;
    }
    const double y = std::log(futures[i]);
    for (std::size_t r = 0; r < kParams; ++r) {
      rhs[r] += row[r] * y;
      for (std::size_t c = 0; c < kParams; ++c) normal[r][c] += row[r] * row[c];
    }
  }
  for (bool s : seen) {
    if (!s) return std::nullopt;
  }
  const auto beta = math::SolveLinear(std::move(normal), std::move(rhs));
  if (!beta) return std::nullopt;
  const std::vector<double>& b = *beta;
  SeasonalCurve curve;
  curve.level = b[0];
  curve.growth = b[1];
  double sum = 0.0;
  for (std::size_t m = 0; m + 1 < kMonths; ++m) {
    curve.factors[m] = b[2 + m];
    sum += curve.factors[m];
  }
  curve.factors[kMonths - 1] = -sum;
  return curve;
}

std::optional<double> SpikeExpectedPrice(const SpikeModel& model, double x0,
                                         double t) {
  if (!ValidSpike(model) || t < 0.0) return std::nullopt;
  const double a = model.a;
  const double mean = x0 * std::exp(-a * t);
  const double variance =
      model.sigma * model.sigma * (1.0 - std::exp(-2.0 * a * t)) / (2.0 * a);
  double jump_part = 0.0;
  if (model.intensity > 0.0 && t > 0.0) {
    // lambda * integral_0^t (E[exp(J e^{-a u})] - 1) du.
    const auto rule = math::GaussLegendre(64, 0.0, t);
    if (!rule) return std::nullopt;
    for (std::size_t k = 0; k < rule->nodes.size(); ++k) {
      const double c = std::exp(-a * rule->nodes[k]);
      const double mgf = std::exp(
          model.jump_mean * c + 0.5 * model.jump_vol * model.jump_vol * c * c);
      jump_part += rule->weights[k] * (mgf - 1.0);
    }
    jump_part *= model.intensity;
  }
  return std::exp(model.log_level + mean + 0.5 * variance + jump_part);
}

std::optional<std::vector<double>> SimulateSpikePath(const SpikeModel& model,
                                                     double x0, double dt,
                                                     int steps,
                                                     std::uint64_t seed) {
  if (!ValidSpike(model) || !(dt > 0.0) || steps <= 0) return std::nullopt;
  SpikeStepper stepper(model, dt, seed);
  std::vector<double> path;
  path.reserve(static_cast<std::size_t>(steps) + 1);
  double x = x0;
  path.push_back(std::exp(model.log_level + x));
  for (int i = 0; i < steps; ++i) {
    x = stepper.Step(x);
    path.push_back(std::exp(model.log_level + x));
  }
  return path;
}

std::optional<SpikeEstimate> SpikeMonteCarlo(const SpikeModel& model, double x0,
                                             double t, int steps, int paths,
                                             std::uint64_t seed) {
  if (!ValidSpike(model) || !(t > 0.0) || steps <= 0 || paths < 2) {
    return std::nullopt;
  }
  SpikeStepper stepper(model, t / steps, seed);
  double sum = 0.0;
  double sum_sq = 0.0;
  for (int p = 0; p < paths; ++p) {
    double x = x0;
    for (int i = 0; i < steps; ++i) x = stepper.Step(x);
    const double s = std::exp(model.log_level + x);
    sum += s;
    sum_sq += s * s;
  }
  const double mean = sum / paths;
  const double variance = (sum_sq / paths - mean * mean) * paths / (paths - 1);
  return SpikeEstimate{.mean = mean, .std_error = std::sqrt(variance / paths)};
}

}  // namespace ofod::models
