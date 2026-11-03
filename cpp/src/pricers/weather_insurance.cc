// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/weather_insurance.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <random>
#include <span>
#include <vector>

#include "ofod/math/linalg.h"
#include "ofod/math/roots.h"

namespace ofod::pricers {
namespace {

constexpr double kYearDays = 365.0;
constexpr double kOmega = 2.0 * std::numbers::pi / kYearDays;

struct Moments {
  double sum = 0.0;
  double sum_sq = 0.0;
  int n = 0;

  void Add(double x) {
    sum += x;
    sum_sq += x * x;
    ++n;
  }
  [[nodiscard]] double Mean() const { return sum / n; }
  [[nodiscard]] double Variance() const {
    const double mean = Mean();
    return std::max((sum_sq / n - mean * mean) * n / (n - 1), 0.0);
  }
  [[nodiscard]] double StdError() const { return std::sqrt(Variance() / n); }
};

bool ValidLosses(const LossModel& m) {
  return m.intensity >= 0.0 && m.severity_sigma >= 0.0;
}

// Aggregate losses over `years` from a compound Poisson process.
class LossSampler {
 public:
  LossSampler(const LossModel& m, double years, std::uint64_t seed)
      : model_(m),
        engine_(seed),
        poisson_(m.intensity * years > 0.0 ? m.intensity * years : 1.0),
        active_(m.intensity * years > 0.0) {}

  double Next() {
    if (!active_) return 0.0;
    const int events = poisson_(engine_);
    double total = 0.0;
    for (int k = 0; k < events; ++k) {
      total += std::exp(model_.severity_mu +
                        model_.severity_sigma * normal_(engine_));
    }
    return total;
  }

 private:
  LossModel model_;
  std::mt19937_64 engine_;
  std::poisson_distribution<int> poisson_;
  std::normal_distribution<double> normal_;
  bool active_;
};

}  // namespace

double DegreeDays(DegreeDayKind kind, std::span<const double> temperatures,
                  double base) {
  double total = 0.0;
  for (double t : temperatures) {
    total += kind == DegreeDayKind::kHeating ? std::max(base - t, 0.0)
                                             : std::max(t - base, 0.0);
  }
  return total;
}

double DegreeDayPayoff(const DegreeDayOption& option, double degree_days) {
  const double intrinsic = option.type == instruments::OptionType::kCall
                               ? std::max(degree_days - option.strike, 0.0)
                               : std::max(option.strike - degree_days, 0.0);
  return std::min(option.tick * intrinsic, option.cap);
}

std::optional<ExpectationEstimate> BurnAnalysis(
    const DegreeDayOption& option, std::span<const double> season_degree_days,
    double rate, double maturity) {
  if (season_degree_days.size() < 2) return std::nullopt;
  Moments m;
  for (double d : season_degree_days) m.Add(DegreeDayPayoff(option, d));
  const double discount = std::exp(-rate * maturity);
  return ExpectationEstimate{.value = discount * m.Mean(),
                             .std_error = discount * m.StdError()};
}

double TemperatureModel::Seasonal(double day) const {
  return mean + trend * day + amplitude * std::cos(kOmega * (day - phase));
}

std::optional<TemperatureModel> FitTemperatureModel(
    std::span<const double> temperatures, int first_day) {
  const std::size_t n = temperatures.size();
  if (n < 60) return std::nullopt;
  constexpr std::size_t kParams = 4;
  math::Matrix normal(kParams, std::vector<double>(kParams, 0.0));
  std::vector<double> rhs(kParams, 0.0);
  auto regressors = [](double d) {
    return std::vector<double>{1.0, d, std::cos(kOmega * d),
                               std::sin(kOmega * d)};
  };
  for (std::size_t i = 0; i < n; ++i) {
    const double d = first_day + static_cast<double>(i);
    const std::vector<double> row = regressors(d);
    for (std::size_t r = 0; r < kParams; ++r) {
      rhs[r] += row[r] * temperatures[i];
      for (std::size_t c = 0; c < kParams; ++c) normal[r][c] += row[r] * row[c];
    }
  }
  const auto beta = math::SolveLinear(std::move(normal), std::move(rhs));
  if (!beta) return std::nullopt;
  TemperatureModel model;
  model.mean = (*beta)[0];
  model.trend = (*beta)[1];
  model.amplitude = std::hypot((*beta)[2], (*beta)[3]);
  double phase = std::atan2((*beta)[3], (*beta)[2]) / kOmega;
  if (phase < 0.0) phase += kYearDays;
  model.phase = phase;
  std::vector<double> residuals;
  residuals.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    residuals.push_back(temperatures[i] -
                        model.Seasonal(first_day + static_cast<double>(i)));
  }
  double sxx = 0.0;
  double sxy = 0.0;
  for (std::size_t i = 0; i + 1 < n; ++i) {
    sxx += residuals[i] * residuals[i];
    sxy += residuals[i] * residuals[i + 1];
  }
  if (!(sxx > 0.0)) return std::nullopt;
  model.phi = sxy / sxx;
  double sse = 0.0;
  for (std::size_t i = 0; i + 1 < n; ++i) {
    const double e = residuals[i + 1] - model.phi * residuals[i];
    sse += e * e;
  }
  model.sigma = std::sqrt(sse / static_cast<double>(n - 2));
  return model;
}

std::optional<DegreeDaySimulation> SimulateDegreeDayOption(
    const TemperatureModel& model, const SeasonSpec& season,
    const DegreeDayOption& option, double rate, double maturity, int paths,
    std::uint64_t seed) {
  if (season.days <= 0 || paths < 2 || model.sigma < 0.0) return std::nullopt;
  std::mt19937_64 engine(seed);
  std::normal_distribution<double> normal;
  std::vector<double> seasonal;
  seasonal.reserve(static_cast<std::size_t>(season.days));
  for (int d = 0; d < season.days; ++d) {
    seasonal.push_back(model.Seasonal(season.first_day + d));
  }
  const bool heating = season.kind == DegreeDayKind::kHeating;
  Moments payoff;
  Moments degree_days;
  for (int p = 0; p < paths; ++p) {
    double x = season.last_deviation;
    double total = 0.0;
    for (double s : seasonal) {
      x = model.phi * x + model.sigma * normal(engine);
      const double t = s + x;
      total += heating ? std::max(season.base - t, 0.0)
                       : std::max(t - season.base, 0.0);
    }
    degree_days.Add(total);
    payoff.Add(DegreeDayPayoff(option, total));
  }
  const double discount = std::exp(-rate * maturity);
  return DegreeDaySimulation{
      .option = {.value = discount * payoff.Mean(),
                 .std_error = discount * payoff.StdError()},
      .mean_degree_days = degree_days.Mean(),
      .std_degree_days = std::sqrt(degree_days.Variance())};
}

std::optional<CatBondValue> PriceCatBond(const CatBondTerms& terms,
                                         const LossModel& losses, double rate,
                                         int paths, std::uint64_t seed) {
  if (!ValidLosses(losses) || paths < 2 || !(terms.maturity > 0.0) ||
      !(terms.principal > 0.0) || terms.frequency <= 0 ||
      !(terms.exhaustion > terms.attachment) || terms.attachment < 0.0) {
    return std::nullopt;
  }
  const int coupons = std::max(
      1, static_cast<int>(std::lround(terms.maturity * terms.frequency)));
  const double coupon = terms.principal * terms.coupon / terms.frequency;
  auto promised = [&](double y) {
    double pv = 0.0;
    for (int k = 1; k <= coupons; ++k) {
      pv += coupon * std::exp(-y * k / static_cast<double>(terms.frequency));
    }
    return pv + terms.principal * std::exp(-y * terms.maturity);
  };
  const double coupon_pv =
      promised(rate) - terms.principal * std::exp(-rate * terms.maturity);
  LossSampler sampler(losses, terms.maturity, seed);
  Moments value;
  Moments written_down;
  int hit = 0;
  const double layer = terms.exhaustion - terms.attachment;
  const double discount = std::exp(-rate * terms.maturity);
  for (int p = 0; p < paths; ++p) {
    const double loss = sampler.Next();
    const double fraction =
        std::clamp((loss - terms.attachment) / layer, 0.0, 1.0);
    if (loss > terms.attachment) ++hit;
    written_down.Add(fraction);
    value.Add(coupon_pv + discount * terms.principal * (1.0 - fraction));
  }
  const double price = value.Mean();
  const auto yield = math::Brent([&](double y) { return promised(y) - price; },
                                 rate - 1.0, rate + 5.0);
  if (!yield) return std::nullopt;
  return CatBondValue{.price = price,
                      .std_error = value.StdError(),
                      .expected_loss = written_down.Mean(),
                      .loss_probability = static_cast<double>(hit) / paths,
                      .spread = *yield - rate};
}

std::optional<ExpectationEstimate> ExcessOfLossLayer(const LossModel& losses,
                                                     double attachment,
                                                     double limit, double years,
                                                     double rate, int paths,
                                                     std::uint64_t seed) {
  if (!ValidLosses(losses) || paths < 2 || !(years > 0.0) || limit < 0.0 ||
      attachment < 0.0) {
    return std::nullopt;
  }
  LossSampler sampler(losses, years, seed);
  Moments payout;
  for (int p = 0; p < paths; ++p) {
    payout.Add(std::min(std::max(sampler.Next() - attachment, 0.0), limit));
  }
  const double discount = std::exp(-rate * years);
  return ExpectationEstimate{.value = discount * payout.Mean(),
                             .std_error = discount * payout.StdError()};
}

}  // namespace ofod::pricers
