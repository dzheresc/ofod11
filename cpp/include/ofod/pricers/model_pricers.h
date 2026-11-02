// Copyright 2026 Denis Zhereshchin
//
// European options under models other than geometric Brownian motion
// (Hull, Ch 27.1-27.3): constant elasticity of variance, Merton's jump
// diffusion and Heston's stochastic volatility.

#ifndef OFOD_PRICERS_MODEL_PRICERS_H_
#define OFOD_PRICERS_MODEL_PRICERS_H_

#include <cstdint>
#include <optional>

#include "ofod/instruments/option.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/monte_carlo.h"

namespace ofod::pricers {

using instruments::OptionType;

// dS = (r - q) S dt + sigma S^alpha dz with sigma = m.volatility, priced
// with the noncentral chi-square formulas of Ch 27.1. Returns std::nullopt
// unless alpha > 0 and |alpha - 1| >= 0.01 (near 1 the noncentrality
// explodes and BSM is the answer anyway).
[[nodiscard]] std::optional<double> CevPrice(OptionType type,
                                             const TreeMarket& m, double strike,
                                             double t, double alpha);

struct JumpParams {
  double intensity;  // lambda, jumps a year.
  double mean_jump;  // k, the expected proportional jump size.
  double jump_vol;   // s, the standard deviation of ln(1 + jump).
};

// Merton's jump diffusion as a Poisson-weighted sum of BSM prices
// (Ch 27.1); m.volatility is the diffusion volatility.
[[nodiscard]] double MertonJumpPrice(OptionType type, const TreeMarket& m,
                                     double strike, double t,
                                     const JumpParams& jumps);

struct HestonParams {
  double v0;     // Initial variance.
  double kappa;  // Mean reversion speed.
  double theta;  // Long-run variance.
  double sigma;  // Volatility of variance.
  double rho;    // Correlation of the two Brownian motions.
};

// Heston's semi-analytic price (Ch 27.2): one Fourier integral of the
// characteristic function in the "little trap" form, integrated with
// 16-point Gauss-Legendre panels of width 1 until three consecutive
// panels add less than 1e-14 (1 + |integral|), at most 2000 panels.
// m.volatility is ignored.
[[nodiscard]] double HestonPrice(OptionType type, const TreeMarket& m,
                                 double strike, double t,
                                 const HestonParams& params);

// Heston by Monte Carlo: log-Euler for S and full-truncation Euler for v.
[[nodiscard]] std::optional<McResult> HestonMonteCarlo(
    OptionType type, const TreeMarket& m, double strike, double t,
    const HestonParams& params, int steps, int paths, std::uint64_t seed);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_MODEL_PRICERS_H_
