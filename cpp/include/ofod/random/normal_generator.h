// Copyright 2026 Denis Zhereshchin
//
// Seeded standard normal draws and correlated normals (Hull, Ch 14).

#ifndef OFOD_RANDOM_NORMAL_GENERATOR_H_
#define OFOD_RANDOM_NORMAL_GENERATOR_H_

#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <vector>

namespace ofod::random {

// Reproducible N(0, 1) draws from a std::mt19937_64 stream. The stream is
// not bit-for-bit identical to the Python package's; models therefore take
// explicit shocks so the two implementations can be compared exactly.
class NormalGenerator {
 public:
  explicit NormalGenerator(std::uint64_t seed) : engine_(seed) {}

  [[nodiscard]] double Next() { return normal_(engine_); }
  [[nodiscard]] std::vector<double> Sample(int n);

 private:
  std::mt19937_64 engine_;
  std::normal_distribution<double> normal_;
};

// Row-major dense matrix; rows must all have the same length.
using Matrix = std::vector<std::vector<double>>;

// Lower-triangular L with L L^T = a. Returns std::nullopt unless `a` is
// square, symmetric and positive definite.
[[nodiscard]] std::optional<Matrix> Cholesky(const Matrix& a);

// Returns L z: independent normals `z` turned into normals with the
// correlation matrix L L^T (Ch 14.5). Requires z.size() == L.size().
[[nodiscard]] std::vector<double> Correlate(const Matrix& lower,
                                            std::span<const double> z);

}  // namespace ofod::random

#endif  // OFOD_RANDOM_NORMAL_GENERATOR_H_
