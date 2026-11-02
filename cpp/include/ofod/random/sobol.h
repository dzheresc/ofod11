// Copyright 2026 Denis Zhereshchin
//
// Sobol low-discrepancy sequence for quasi-Monte Carlo (Hull, Ch 21.7).

#ifndef OFOD_RANDOM_SOBOL_H_
#define OFOD_RANDOM_SOBOL_H_

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace ofod::random {

// Points in [0, 1)^d generated with Gray-code ordering and the first eight
// Joe-Kuo direction numbers. The all-zero first point is skipped.
class Sobol {
 public:
  static constexpr int kMaxDimensions = 8;

  // Returns std::nullopt unless 1 <= dimensions <= kMaxDimensions.
  [[nodiscard]] static std::optional<Sobol> Create(int dimensions);

  [[nodiscard]] std::vector<double> Next();
  [[nodiscard]] int dimensions() const { return dimensions_; }

 private:
  static constexpr int kBits = 32;
  explicit Sobol(int dimensions);

  int dimensions_;
  std::uint32_t index_ = 0;
  std::vector<std::array<std::uint32_t, kBits>> directions_;
  std::vector<std::uint32_t> state_;
};

}  // namespace ofod::random

#endif  // OFOD_RANDOM_SOBOL_H_
