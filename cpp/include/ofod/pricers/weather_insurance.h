// Copyright 2026 Denis Zhereshchin
//
// Weather and insurance derivatives (Hull, Ch 35.5-35.6): heating and
// cooling degree days, degree-day options priced by historical burn
// analysis and by simulating a mean-reverting seasonal temperature model,
// and catastrophe bonds and reinsurance layers on a compound Poisson loss
// process. Both risks are taken to be uncorrelated with the market, so
// they are priced as discounted real-world expectations.

#ifndef OFOD_PRICERS_WEATHER_INSURANCE_H_
#define OFOD_PRICERS_WEATHER_INSURANCE_H_

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "ofod/instruments/option.h"

namespace ofod::pricers {

enum class DegreeDayKind { kHeating, kCooling };

// Sum over days of max(base - T, 0) (heating) or max(T - base, 0)
// (cooling), with Hull's base of 65 degrees Fahrenheit.
[[nodiscard]] double DegreeDays(DegreeDayKind kind,
                                std::span<const double> temperatures,
                                double base = 65.0);

// A call (kCall) pays tick * max(D - strike, 0) and a put tick *
// max(strike - D, 0), capped at `cap`, where D is the season's degree days.
struct DegreeDayOption {
  instruments::OptionType type = instruments::OptionType::kCall;
  double strike = 0.0;
  double tick = 1.0;
  double cap = std::numeric_limits<double>::infinity();
};

[[nodiscard]] double DegreeDayPayoff(const DegreeDayOption& option,
                                     double degree_days);

struct ExpectationEstimate {
  double value;      // Discounted mean payoff.
  double std_error;  // Of the discounted mean.
};

// Burn analysis: the discounted average payoff over historical seasons.
// Returns nullopt for fewer than two seasons.
[[nodiscard]] std::optional<ExpectationEstimate> BurnAnalysis(
    const DegreeDayOption& option, std::span<const double> season_degree_days,
    double rate, double maturity);

// Daily temperature T_d = mean + trend d + amplitude cos(2 pi (d - phase) /
// 365) + x_d, with x_{d+1} = phi x_d + sigma eps (an AR(1) deviation).
struct TemperatureModel {
  double mean = 0.0;
  double trend = 0.0;
  double amplitude = 0.0;
  double phase = 0.0;  // Day of the year of the seasonal peak.
  double phi = 0.0;
  double sigma = 0.0;

  [[nodiscard]] double Seasonal(double day) const;
};

// Fits the model to daily temperatures for days first_day, first_day + 1,
// ... by least squares on (1, d, cos, sin) and an AR(1) regression on the
// residuals. Needs at least 60 observations.
[[nodiscard]] std::optional<TemperatureModel> FitTemperatureModel(
    std::span<const double> temperatures, int first_day = 0);

// Simulates `days` days starting at `first_day` (with x_{first_day - 1} =
// last_deviation) and prices the option on the season's degree days.
struct SeasonSpec {
  DegreeDayKind kind = DegreeDayKind::kHeating;
  int first_day = 0;
  int days = 0;
  double last_deviation = 0.0;
  double base = 65.0;
};

struct DegreeDaySimulation {
  ExpectationEstimate option;
  double mean_degree_days;
  double std_degree_days;
};

[[nodiscard]] std::optional<DegreeDaySimulation> SimulateDegreeDayOption(
    const TemperatureModel& model, const SeasonSpec& season,
    const DegreeDayOption& option, double rate, double maturity, int paths,
    std::uint64_t seed);

// Aggregate losses over a period: Poisson(intensity * years) events with
// lognormal severities, ln X ~ N(severity_mu, severity_sigma^2).
struct LossModel {
  double intensity = 0.0;  // Events per year.
  double severity_mu = 0.0;
  double severity_sigma = 0.0;
};

// A catastrophe bond: principal is written down linearly as aggregate
// losses over its life rise from `attachment` to `exhaustion`, and the
// bond pays an annual coupon (on the original principal) `frequency` times
// a year and the remaining principal at maturity.
struct CatBondTerms {
  double principal = 100.0;
  double coupon = 0.0;
  double maturity = 1.0;
  double attachment = 0.0;
  double exhaustion = 0.0;
  int frequency = 1;
};

struct CatBondValue {
  double price;
  double std_error;
  double expected_loss;     // Expected principal written down / principal.
  double loss_probability;  // P(losses exceed the attachment).
  double spread;  // Yield spread over the risk-free rate (cont. comp.).
};

// Monte Carlo value with the risk-free rate `rate`. The spread is the
// constant y - r that reprices the bond's promised cash flows to the
// estimated price. Returns nullopt for bad terms.
[[nodiscard]] std::optional<CatBondValue> PriceCatBond(
    const CatBondTerms& terms, const LossModel& losses, double rate, int paths,
    std::uint64_t seed);

// Expected discounted cost of an excess-of-loss reinsurance layer paying
// min(max(L - attachment, 0), limit) at the end of a period of `years`.
[[nodiscard]] std::optional<ExpectationEstimate> ExcessOfLossLayer(
    const LossModel& losses, double attachment, double limit, double years,
    double rate, int paths, std::uint64_t seed);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_WEATHER_INSURANCE_H_
