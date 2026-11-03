// Copyright 2026 Denis Zhereshchin

#include "ofod/random/normal_generator.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "ofod/math/normal.h"
#include "ofod/stats/descriptive.h"

namespace ofod::random {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Seeded streams are reproducible and standard normal",
          "[random][ch14]") {
  NormalGenerator a(42);
  NormalGenerator b(42);
  CHECK(a.Sample(5) == b.Sample(5));
  const auto x = NormalGenerator(7).Sample(200000);
  CHECK_THAT(*stats::Mean(x), WithinAbs(0.0, 0.01));
  CHECK_THAT(*stats::Variance(x), WithinAbs(1.0, 0.01));
}

TEST_CASE("xoshiro256++ matches the reference sequence", "[random]") {
  // SplitMix64(0) state through the authors' xoshiro256++ step (computed
  // independently; the same step gives 41943041, 58720359, ... from the
  // state {1, 2, 3, 4}).
  Xoshiro256 gen(0);
  CHECK(gen() == 0x53175d61490b23dfULL);
  CHECK(gen() == 0x61da6f3dc380d507ULL);
  CHECK(gen() == 0x5c0fdf91ec9a7bfcULL);
}

TEST_CASE("Ziggurat draws are standard normal", "[random]") {
  ZigguratNormalGenerator a(42);
  ZigguratNormalGenerator b(42);
  CHECK(a.Sample(5) == b.Sample(5));
  CHECK(ZigguratNormalGenerator(1).Sample(5) !=
        ZigguratNormalGenerator(2).Sample(5));

  constexpr int kN = 1'000'000;
  auto x = ZigguratNormalGenerator(7).Sample(kN);
  double m1 = 0.0;
  double m2 = 0.0;
  double m3 = 0.0;
  double m4 = 0.0;
  int tail = 0;
  for (double v : x) {
    m1 += v;
    m2 += v * v;
    m3 += v * v * v;
    m4 += v * v * v * v;
    if (std::abs(v) > 3.6541528853610088) ++tail;  // Beyond the base strip.
  }
  // Standard errors of the moments: 1, sqrt(2), sqrt(15), sqrt(96) over
  // sqrt(n); allow five of them.
  const double root_n = std::sqrt(static_cast<double>(kN));
  CHECK_THAT(m1 / kN, WithinAbs(0.0, 5.0 / root_n));
  CHECK_THAT(m2 / kN, WithinAbs(1.0, 5.0 * std::sqrt(2.0) / root_n));
  CHECK_THAT(m3 / kN, WithinAbs(0.0, 5.0 * std::sqrt(15.0) / root_n));
  CHECK_THAT(m4 / kN, WithinAbs(3.0, 5.0 * std::sqrt(96.0) / root_n));
  // P(|Z| > R) = 2.58e-4: the tail branch is exercised and has the right
  // weight (about 258 +- 16 draws).
  CHECK(tail > 258 - 5 * 16);
  CHECK(tail < 258 + 5 * 16);

  // Kolmogorov-Smirnov against N(0, 1): the 0.1% critical value is
  // 1.95 / sqrt(n).
  std::sort(x.begin(), x.end());
  double d = 0.0;
  for (int i = 0; i < kN; ++i) {
    const double cdf = math::NormCdf(x[static_cast<std::size_t>(i)]);
    d = std::max({d, (i + 1.0) / kN - cdf, cdf - static_cast<double>(i) / kN});
  }
  CHECK(d < 1.95 / root_n);
}

TEST_CASE("Cholesky reproduces the matrix", "[random][ch14]") {
  const Matrix a = {{4.0, 2.0, 0.4}, {2.0, 2.0, 0.5}, {0.4, 0.5, 3.0}};
  const auto l = Cholesky(a);
  REQUIRE(l.has_value());
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      double sum = 0.0;
      for (std::size_t k = 0; k < 3; ++k) sum += (*l)[i][k] * (*l)[j][k];
      CHECK_THAT(sum, WithinAbs(a[i][j], 1e-12));
    }
  }
  CHECK_FALSE(Cholesky({{1.0, 2.0}, {2.0, 1.0}}));  // Not positive definite.
  CHECK_FALSE(Cholesky({{1.0, 0.5}, {0.4, 1.0}}));  // Not symmetric.
  CHECK_FALSE(Cholesky({{1.0, 0.5}}));              // Not square.
}

TEST_CASE("Correlated draws have the target correlation", "[random][ch14]") {
  const auto l = Cholesky({{1.0, -0.7}, {-0.7, 1.0}});
  REQUIRE(l.has_value());
  NormalGenerator gen(11);
  std::vector<double> x;
  std::vector<double> y;
  for (int i = 0; i < 100000; ++i) {
    const std::vector<double> z = {gen.Next(), gen.Next()};
    const auto e = Correlate(*l, z);
    x.push_back(e[0]);
    y.push_back(e[1]);
  }
  CHECK_THAT(*stats::Correlation(x, y), WithinAbs(-0.7, 0.01));
}

}  // namespace
}  // namespace ofod::random
