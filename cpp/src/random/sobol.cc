// Copyright 2026 Denis Zhereshchin

#include "ofod/random/sobol.h"

#include <bit>
#include <cstddef>

namespace ofod::random {

namespace {

// Joe-Kuo parameters for dimensions 2..8: degree s, coefficients a of the
// primitive polynomial, and initial direction integers m.
struct Primitive {
  int degree;
  std::uint32_t a;
  std::array<std::uint32_t, 5> m;
};

constexpr std::array<Primitive, 7> kPrimitives = {{
    {1, 0, {1, 0, 0, 0, 0}},
    {2, 1, {1, 3, 0, 0, 0}},
    {3, 1, {1, 3, 1, 0, 0}},
    {3, 2, {1, 1, 1, 0, 0}},
    {4, 1, {1, 1, 3, 3, 0}},
    {4, 4, {1, 3, 5, 13, 0}},
    {5, 2, {1, 1, 5, 5, 17}},
}};

}  // namespace

std::optional<Sobol> Sobol::Create(int dimensions) {
  if (dimensions < 1 || dimensions > kMaxDimensions) return std::nullopt;
  return Sobol(dimensions);
}

Sobol::Sobol(int dimensions)
    : dimensions_(dimensions),
      directions_(static_cast<std::size_t>(dimensions)),
      state_(static_cast<std::size_t>(dimensions), 0) {
  // Dimension 1: v_k = 2^(32 - k), i.e. the van der Corput sequence.
  for (int k = 0; k < kBits; ++k) {
    directions_[0][static_cast<std::size_t>(k)] = 1U << (kBits - 1 - k);
  }
  for (int d = 1; d < dimensions; ++d) {
    const Primitive& p = kPrimitives[static_cast<std::size_t>(d - 1)];
    auto& v = directions_[static_cast<std::size_t>(d)];
    const int s = p.degree;
    for (int k = 0; k < s; ++k) {
      v[static_cast<std::size_t>(k)] = p.m[static_cast<std::size_t>(k)]
                                       << (kBits - 1 - k);
    }
    for (int k = s; k < kBits; ++k) {
      const auto uk = static_cast<std::size_t>(k);
      std::uint32_t value = v[uk - static_cast<std::size_t>(s)] ^
                            (v[uk - static_cast<std::size_t>(s)] >> s);
      for (int j = 1; j < s; ++j) {
        if ((p.a >> (s - 1 - j)) & 1U) {
          value ^= v[uk - static_cast<std::size_t>(j)];
        }
      }
      v[uk] = value;
    }
  }
}

std::vector<double> Sobol::Next() {
  // Gray code: flip the direction number of the lowest zero bit of index.
  const int c = std::countr_one(index_);
  ++index_;
  std::vector<double> point(static_cast<std::size_t>(dimensions_));
  constexpr double kScale = 1.0 / 4294967296.0;  // 2^-32.
  for (std::size_t d = 0; d < point.size(); ++d) {
    state_[d] ^= directions_[d][static_cast<std::size_t>(c)];
    point[d] = static_cast<double>(state_[d]) * kScale;
  }
  return point;
}

}  // namespace ofod::random
