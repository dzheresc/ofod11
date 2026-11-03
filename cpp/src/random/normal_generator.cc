// Copyright 2026 Denis Zhereshchin

#include "ofod/random/normal_generator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace ofod::random {

std::vector<double> NormalGenerator::Sample(int n) {
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(n > 0 ? n : 0));
  for (int i = 0; i < n; ++i) out.push_back(Next());
  return out;
}

namespace {

// Ziggurat tail start for 256 layers (Marsaglia and Tsang, 2000).
constexpr double kZigguratR = 3.6541528853610088;

const ZigguratNormalGenerator::Table& BuildZigguratTable() {
  static const ZigguratNormalGenerator::Table table = [] {
    auto f = [](double x) { return std::exp(-0.5 * x * x); };
    // Area of each layer: the base strip R f(R) plus the tail beyond R.
    const double v = kZigguratR * f(kZigguratR) +
                     std::sqrt(std::numbers::pi / 2.0) *
                         std::erfc(kZigguratR / std::numbers::sqrt2);
    ZigguratNormalGenerator::Table t{};
    t.x[0] = v / f(kZigguratR);
    t.x[1] = kZigguratR;
    for (std::size_t i = 1; i < 255; ++i) {
      t.x[i + 1] = std::sqrt(-2.0 * std::log(v / t.x[i] + f(t.x[i])));
    }
    t.x[256] = 0.0;
    for (std::size_t i = 0; i < 257; ++i) t.f[i] = f(t.x[i]);
    return t;
  }();
  return table;
}

}  // namespace

Xoshiro256::Xoshiro256(std::uint64_t seed) {
  // SplitMix64, as the xoshiro authors recommend for seeding.
  for (auto& word : s_) {
    seed += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = seed;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    word = z ^ (z >> 31);
  }
}

ZigguratNormalGenerator::ZigguratNormalGenerator(std::uint64_t seed)
    : engine_(seed), table_(&BuildZigguratTable()) {}

double ZigguratNormalGenerator::Slow(std::size_t layer, double x,
                                     bool negative) {
  for (;;) {
    if (layer == 0) {
      // Tail beyond R (Marsaglia, 1964).
      double a = 0.0;
      double b = 0.0;
      do {
        a = -std::log(Uniform()) / kZigguratR;
        b = -std::log(Uniform());
      } while (2.0 * b < a * a);
      return negative ? -(kZigguratR + a) : kZigguratR + a;
    }
    // Wedge: accept if a uniform height in the layer is under the curve.
    const double y = table_->f[layer] +
                     Uniform() * (table_->f[layer + 1] - table_->f[layer]);
    if (y < std::exp(-0.5 * x * x)) return negative ? -x : x;
    // Rejected: draw again from scratch.
    const std::uint64_t bits = engine_();
    layer = static_cast<std::size_t>(bits & 0xFF);
    negative = (bits & 0x100) != 0;
    x = static_cast<double>(bits >> 11) * 0x1.0p-53 * table_->x[layer];
    if (x < table_->x[layer + 1]) return negative ? -x : x;
  }
}

std::vector<double> ZigguratNormalGenerator::Sample(int n) {
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(n > 0 ? n : 0));
  for (int i = 0; i < n; ++i) out.push_back(Next());
  return out;
}

std::optional<Matrix> Cholesky(const Matrix& a) {
  const std::size_t n = a.size();
  for (const auto& row : a) {
    if (row.size() != n) return std::nullopt;
  }
  constexpr double kSymmetryTolerance = 1e-12;
  Matrix l(n, std::vector<double>(n, 0.0));
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j <= i; ++j) {
      if (std::abs(a[i][j] - a[j][i]) > kSymmetryTolerance) return std::nullopt;
      double sum = a[i][j];
      for (std::size_t k = 0; k < j; ++k) sum -= l[i][k] * l[j][k];
      if (i == j) {
        if (sum <= 0.0) return std::nullopt;
        l[i][i] = std::sqrt(sum);
      } else {
        l[i][j] = sum / l[j][j];
      }
    }
  }
  return l;
}

std::vector<double> Correlate(const Matrix& lower, std::span<const double> z) {
  std::vector<double> out(lower.size(), 0.0);
  for (std::size_t i = 0; i < lower.size(); ++i) {
    // A local accumulator keeps the sum in a register; accumulating into
    // out[i] made the compiler store and reload it on every term. The
    // summation order is unchanged, so results are bit-for-bit the same.
    const std::vector<double>& row = lower[i];
    const std::size_t terms = std::min(i + 1, z.size());
    double sum = 0.0;
    for (std::size_t k = 0; k < terms; ++k) sum += row[k] * z[k];
    out[i] = sum;
  }
  return out;
}

}  // namespace ofod::random
