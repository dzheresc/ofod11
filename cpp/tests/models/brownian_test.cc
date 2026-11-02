// Copyright 2026 Denis Zhereshchin

#include "ofod/models/brownian.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "ofod/random/normal_generator.h"
#include "ofod/stats/descriptive.h"

namespace ofod::models {
namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("Quadratic variation of a Wiener path tends to T", "[models][ch14]") {
  const ArithmeticBrownianMotion w{.drift = 0.0, .volatility = 1.0};
  const int n = 200000;
  const auto path = w.Path(0.0, 2.0 / n, random::NormalGenerator(3).Sample(n));
  CHECK_THAT(QuadraticVariation(path), WithinAbs(2.0, 0.02));
}

TEST_CASE("Exact GBM terminal values are lognormal", "[models][ch15]") {
  const GeometricBrownianMotion gbm{.drift = 0.1, .volatility = 0.3};
  random::NormalGenerator gen(5);
  std::vector<double> logs;
  for (int i = 0; i < 50000; ++i) {
    const std::vector<double> eps = {gen.Next()};
    logs.push_back(std::log(gbm.ExactPath(50.0, 2.0, eps).back()));
  }
  CHECK_THAT(*stats::Mean(logs), WithinAbs(gbm.LogMean(50.0, 2.0), 0.01));
  CHECK_THAT(*stats::StdDev(logs), WithinRel(gbm.LogStdDev(2.0), 0.01));
}

TEST_CASE("Ito's lemma: ln S drifts at mu - sigma^2/2", "[models][ch14]") {
  // Euler paths on a fine grid: the mean of ln S_T matches eq. 14.21.
  const GeometricBrownianMotion gbm{.drift = 0.12, .volatility = 0.4};
  random::NormalGenerator gen(9);
  double sum = 0.0;
  const int paths = 4000;
  for (int i = 0; i < paths; ++i) {
    sum += std::log(gbm.EulerPath(1.0, 1.0 / 250, gen.Sample(250)).back());
  }
  CHECK_THAT(sum / paths, WithinAbs(0.12 - 0.08, 0.02));
}

}  // namespace
}  // namespace ofod::models
