// Copyright 2026 Denis Zhereshchin
//
// Seeded standard normal draws and correlated normals (Hull, Ch 14).

#ifndef OFOD_RANDOM_NORMAL_GENERATOR_H_
#define OFOD_RANDOM_NORMAL_GENERATOR_H_

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
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

// xoshiro256++ (Blackman and Vigna, 2019): a small, fast 64-bit generator
// with a 2^256 - 1 period. The state is filled from the seed by SplitMix64.
class Xoshiro256 {
 public:
  using result_type = std::uint64_t;

  explicit Xoshiro256(std::uint64_t seed);

  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return ~result_type{0}; }

  result_type operator()() {
    const std::uint64_t result = Rotl(s_[0] + s_[3], 23) + s_[0];
    const std::uint64_t t = s_[1] << 17;
    s_[2] ^= s_[0];
    s_[3] ^= s_[1];
    s_[1] ^= s_[2];
    s_[0] ^= s_[3];
    s_[2] ^= t;
    s_[3] = Rotl(s_[3], 45);
    return result;
  }

 private:
  static std::uint64_t Rotl(std::uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
  }

  std::array<std::uint64_t, 4> s_;
};

// N(0, 1) draws by the Marsaglia-Tsang ziggurat (256 layers) on
// xoshiro256++. About 99% of draws cost one 64-bit integer and one
// multiply, against a logarithm, a square root and two uniforms per pair
// for std::normal_distribution. The layer index and sign come from the
// low 9 bits and the uniform from the top 53, so they are independent
// (Doornik, 2005). A different stream from NormalGenerator for the same
// seed.
class ZigguratNormalGenerator {
 public:
  explicit ZigguratNormalGenerator(std::uint64_t seed);

  [[nodiscard]] double Next() {
    const std::uint64_t bits = engine_();
    const auto layer = static_cast<std::size_t>(bits & 0xFF);
    const double u = static_cast<double>(bits >> 11) * 0x1.0p-53;
    const double x = u * table_->x[layer];
    if (x < table_->x[layer + 1]) [[likely]] {
      // Bit 8 of `bits` becomes the sign bit, without a branch on a coin
      // flip.
      return std::bit_cast<double>(std::bit_cast<std::uint64_t>(x) ^
                                   ((bits & 0x100) << 55));
    }
    return Slow(layer, x, (bits & 0x100) != 0);
  }

  [[nodiscard]] std::vector<double> Sample(int n);

  // Layer edges x[0] > x[1] = R > ... > x[256] = 0 and f(x) = e^{-x^2/2}
  // at each edge; layer i has width x[i] and spans f(x[i]) to f(x[i+1]).
  struct Table {
    std::array<double, 257> x;
    std::array<double, 257> f;
  };

 private:
  double Slow(std::size_t layer, double x, bool negative);
  double Uniform() {
    return (static_cast<double>(engine_() >> 11) + 0.5) * 0x1.0p-53;
  }

  Xoshiro256 engine_;
  const Table* table_;
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
