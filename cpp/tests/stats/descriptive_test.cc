// Copyright 2026 Denis Zhereshchin

#include "ofod/stats/descriptive.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>

namespace ofod::stats {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Sample moments", "[stats]") {
  const std::vector<double> x = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
  CHECK_THAT(*Mean(x), WithinAbs(5.0, 1e-15));
  CHECK_THAT(*Variance(x), WithinAbs(32.0 / 7.0, 1e-14));
  CHECK_FALSE(Variance(std::vector<double>{1.0}));
  CHECK_FALSE(Mean(std::vector<double>{}));
}

TEST_CASE("Correlation and regression", "[stats][ch03]") {
  const std::vector<double> x = {1, 2, 3, 4, 5};
  const std::vector<double> y = {2.1, 3.9, 6.2, 7.8, 10.1};
  const auto fit = LinearRegression(x, y);
  REQUIRE(fit.has_value());
  CHECK_THAT(fit->slope, WithinAbs(1.99, 1e-12));
  CHECK_THAT(fit->r_squared,
             WithinAbs(*Correlation(x, y) * *Correlation(x, y), 1e-12));
  const std::vector<double> flat = {1, 1, 1, 1, 1};
  CHECK_FALSE(Correlation(x, flat));
}

}  // namespace
}  // namespace ofod::stats
