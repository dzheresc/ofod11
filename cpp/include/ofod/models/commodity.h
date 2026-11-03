// Copyright 2026 Denis Zhereshchin
//
// Commodity price models (Hull, Ch 35.3-35.4): a seasonal forward curve
// fitted to futures prices and a mean-reverting log-price with jumps for
// spiky markets such as electricity.

#ifndef OFOD_MODELS_COMMODITY_H_
#define OFOD_MODELS_COMMODITY_H_

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ofod::models {

// ln F(t) = level + growth t + factor[m(t)], where m(t) is the calendar
// month floor(12 t) mod 12 (t = 0 is the start of month 0) and the twelve
// factors sum to zero.
struct SeasonalCurve {
  double level = 0.0;
  double growth = 0.0;
  std::array<double, 12> factors{};

  [[nodiscard]] double Forward(double t) const;
  // Seasonal multiplier exp(factor) of the month containing t.
  [[nodiscard]] double Multiplier(double t) const;
};

// Least-squares fit of ln F on time and month dummies. Needs at least 14
// futures with every calendar month represented and positive prices;
// returns nullopt otherwise.
[[nodiscard]] std::optional<SeasonalCurve> FitSeasonalCurve(
    std::span<const double> maturities, std::span<const double> futures);

// ln S(t) = f(t) + x(t), dx = -a x dt + sigma dz + J dN with Poisson jumps
// of intensity `intensity` and J ~ N(jump_mean, jump_vol^2). f is chosen so
// that ln S has the long-run level `log_level` when x = 0 (Ch 35.4).
struct SpikeModel {
  double log_level = 0.0;
  double a = 0.0;
  double sigma = 0.0;
  double intensity = 0.0;
  double jump_mean = 0.0;
  double jump_vol = 0.0;
};

// E[S(T)] given x(0) = x0 by the closed-form moment generating function
// of the Ornstein-Uhlenbeck part and Gauss-Legendre integration of the
// jump part. Returns nullopt if a <= 0 or T < 0.
[[nodiscard]] std::optional<double> SpikeExpectedPrice(const SpikeModel& model,
                                                       double x0, double t);

// One simulated path of S on `steps` steps of length dt, with the OU part
// sampled exactly and at most the Poisson number of jumps per step added
// at the step's end. Element 0 is S(0). Returns nullopt for bad inputs.
[[nodiscard]] std::optional<std::vector<double>> SimulateSpikePath(
    const SpikeModel& model, double x0, double dt, int steps,
    std::uint64_t seed);

struct SpikeEstimate {
  double mean;
  double std_error;
};

// Monte Carlo estimate of E[S(T)] with the same scheme as the path.
[[nodiscard]] std::optional<SpikeEstimate> SpikeMonteCarlo(
    const SpikeModel& model, double x0, double t, int steps, int paths,
    std::uint64_t seed);

}  // namespace ofod::models

#endif  // OFOD_MODELS_COMMODITY_H_
