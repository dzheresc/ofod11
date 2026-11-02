// Copyright 2026 Denis Zhereshchin
//
// Descriptive statistics and simple linear regression.

#ifndef OFOD_STATS_DESCRIPTIVE_H_
#define OFOD_STATS_DESCRIPTIVE_H_

#include <optional>
#include <span>
#include <vector>

namespace ofod::stats {

// Arithmetic mean; nullopt for an empty sample.
[[nodiscard]] std::optional<double> Mean(std::span<const double> x);

// Unbiased sample variance (divides by n - 1); nullopt when n < 2.
[[nodiscard]] std::optional<double> Variance(std::span<const double> x);

// Sample standard deviation; nullopt when n < 2.
[[nodiscard]] std::optional<double> StdDev(std::span<const double> x);

// Unbiased sample covariance; nullopt when sizes differ or n < 2.
[[nodiscard]] std::optional<double> Covariance(std::span<const double> x,
                                               std::span<const double> y);

// Pearson correlation; nullopt when sizes differ, n < 2 or a series is flat.
[[nodiscard]] std::optional<double> Correlation(std::span<const double> x,
                                                std::span<const double> y);

struct Regression {
  double intercept;
  double slope;
  double r_squared;
};

// Ordinary least squares fit of y = intercept + slope * x.
[[nodiscard]] std::optional<Regression> LinearRegression(
    std::span<const double> x, std::span<const double> y);

// ln(S_i / S_{i-1}) for consecutive prices (Ch 15.4).
[[nodiscard]] std::vector<double> LogReturns(std::span<const double> prices);

// Annualised volatility: the sample standard deviation of log returns times
// sqrt(periods_per_year), e.g. 252 for daily data (Ch 15.4). Returns
// std::nullopt with fewer than three prices.
[[nodiscard]] std::optional<double> HistoricalVolatility(
    std::span<const double> prices, double periods_per_year);

}  // namespace ofod::stats

#endif  // OFOD_STATS_DESCRIPTIVE_H_
