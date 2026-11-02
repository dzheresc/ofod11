// Copyright 2026 Denis Zhereshchin
//
// Implied volatility surface on a (maturity x strike) grid (Hull, Ch 20.6).

#ifndef OFOD_CURVES_VOL_SURFACE_H_
#define OFOD_CURVES_VOL_SURFACE_H_

#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ofod::curves {

enum class VolInterpolation {
  kTotalVariance,  // Linear in sigma^2 T across maturities (no calendar
                   // arbitrage between nodes).
  kLinearVol,      // Linear in sigma across maturities (Hull's Table 20.2).
};

class VolSurface {
 public:
  // `volatilities[i][j]` is the implied volatility for maturities[i] and
  // strikes[j]. Returns std::nullopt unless both axes are non-empty and
  // strictly increasing, maturities are positive, every row has one entry
  // per strike and every volatility is positive.
  static std::optional<VolSurface> Create(
      std::vector<double> maturities, std::vector<double> strikes,
      std::vector<std::vector<double>> volatilities,
      VolInterpolation interpolation = VolInterpolation::kTotalVariance);

  // Linear in strike, then across maturities by `interpolation()`; flat
  // beyond the grid on both axes.
  [[nodiscard]] double Volatility(double strike, double t) const;

  std::span<const double> maturities() const { return maturities_; }
  std::span<const double> strikes() const { return strikes_; }
  const std::vector<std::vector<double>>& volatilities() const {
    return volatilities_;
  }
  VolInterpolation interpolation() const { return interpolation_; }

 private:
  VolSurface(std::vector<double> maturities, std::vector<double> strikes,
             std::vector<std::vector<double>> volatilities,
             VolInterpolation interpolation)
      : maturities_(std::move(maturities)),
        strikes_(std::move(strikes)),
        volatilities_(std::move(volatilities)),
        interpolation_(interpolation) {}

  // Volatility of row `i` at `strike`.
  [[nodiscard]] double RowVolatility(std::size_t i, double strike) const;

  std::vector<double> maturities_;
  std::vector<double> strikes_;
  std::vector<std::vector<double>> volatilities_;
  VolInterpolation interpolation_;
};

}  // namespace ofod::curves

#endif  // OFOD_CURVES_VOL_SURFACE_H_
