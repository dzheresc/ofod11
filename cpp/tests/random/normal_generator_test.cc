// Copyright 2026 Denis Zhereshchin

#include "ofod/random/normal_generator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>

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
