// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/finite_difference.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ofod::pricers {

namespace {

using instruments::Exercise;
using instruments::IntrinsicValue;
using instruments::OptionType;
using instruments::VanillaOption;

// Tridiagonal coefficients of the spatial operator L f_j = a f_{j-1} +
// b f_j + c f_{j+1} without the -r f term.
struct Stencil {
  double a;
  double b;
  double c;
};

// Solves the tridiagonal system with sub-diagonal `lo`, diagonal `mid`,
// super-diagonal `hi` (Thomas algorithm). Sizes match `rhs`.
std::vector<double> SolveTridiagonal(std::vector<double> lo,
                                     std::vector<double> mid,
                                     std::vector<double> hi,
                                     std::vector<double> rhs) {
  const std::size_t n = rhs.size();
  for (std::size_t i = 1; i < n; ++i) {
    const double w = lo[i] / mid[i - 1];
    mid[i] -= w * hi[i - 1];
    rhs[i] -= w * rhs[i - 1];
  }
  std::vector<double> x(n);
  x[n - 1] = rhs[n - 1] / mid[n - 1];
  for (std::size_t i = n - 1; i-- > 0;) {
    x[i] = (rhs[i] - hi[i] * x[i + 1]) / mid[i];
  }
  return x;
}

// Projected SOR for the American LCP: min over x >= floor of the system.
void Psor(const std::vector<double>& lo, const std::vector<double>& mid,
          const std::vector<double>& hi, const std::vector<double>& rhs,
          const std::vector<double>& floor, std::vector<double>& x) {
  constexpr double kOmega = 1.2;
  constexpr double kTolerance = 1e-12;
  constexpr int kMaxIterations = 20000;
  const std::size_t n = rhs.size();
  for (int it = 0; it < kMaxIterations; ++it) {
    double change = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      double r = rhs[i];
      if (i > 0) r -= lo[i] * x[i - 1];
      if (i + 1 < n) r -= hi[i] * x[i + 1];
      const double gauss = r / mid[i];
      const double next = std::max(floor[i], x[i] + kOmega * (gauss - x[i]));
      change = std::max(change, std::abs(next - x[i]));
      x[i] = next;
    }
    if (change < kTolerance) return;
  }
}

// True if the tridiagonal matrix is an M-matrix with a diagonally dominant
// diagonal: the condition under which Brennan-Schwartz solves the LCP.
bool IsDominantMMatrix(const std::vector<double>& lo,
                       const std::vector<double>& mid,
                       const std::vector<double>& hi) {
  for (std::size_t i = 0; i < mid.size(); ++i) {
    if (lo[i] > 0.0 || hi[i] > 0.0 || !(mid[i] >= -lo[i] - hi[i])) {
      return false;
    }
  }
  return true;
}

// Brennan-Schwartz: one elimination sweep and one projected substitution
// sweep. For a put the exercise region is the bottom of the grid, so the
// system is eliminated upwards and substituted from the bottom, projecting
// each value as it is found; a call runs the usual Thomas algorithm and
// projects on the way back down. Exact for a single exercise boundary and
// an M-matrix (Jaillet, Lamberton and Lapeyre, 1990).
std::vector<double> BrennanSchwartz(std::vector<double> lo,
                                    std::vector<double> mid,
                                    std::vector<double> hi,
                                    std::vector<double> rhs,
                                    const std::vector<double>& floor,
                                    bool exercise_at_top) {
  const std::size_t n = rhs.size();
  std::vector<double> x(n);
  if (exercise_at_top) {
    for (std::size_t i = 1; i < n; ++i) {
      const double w = lo[i] / mid[i - 1];
      mid[i] -= w * hi[i - 1];
      rhs[i] -= w * rhs[i - 1];
    }
    x[n - 1] = std::max(floor[n - 1], rhs[n - 1] / mid[n - 1]);
    for (std::size_t i = n - 1; i-- > 0;) {
      x[i] = std::max(floor[i], (rhs[i] - hi[i] * x[i + 1]) / mid[i]);
    }
  } else {
    for (std::size_t i = n - 1; i-- > 0;) {
      const double w = hi[i] / mid[i + 1];
      mid[i] -= w * lo[i + 1];
      rhs[i] -= w * rhs[i + 1];
    }
    x[0] = std::max(floor[0], rhs[0] / mid[0]);
    for (std::size_t i = 1; i < n; ++i) {
      x[i] = std::max(floor[i], (rhs[i] - lo[i] * x[i - 1]) / mid[i]);
    }
  }
  return x;
}

}  // namespace

std::optional<double> PriceFiniteDifference(const VanillaOption& option,
                                            const TreeMarket& market,
                                            const FdGrid& grid, FdScheme scheme,
                                            AmericanSolver solver) {
  const int m = grid.price_steps;
  const int n = grid.time_steps;
  if (m < 3 || n < 1 || !(grid.s_max > grid.s_min) ||
      market.spot < grid.s_min || market.spot > grid.s_max ||
      (grid.log_space && grid.s_min <= 0.0) || grid.s_min < 0.0) {
    return std::nullopt;
  }
  const double r = market.rate;
  const double q = market.yield;
  const double sigma = market.volatility;
  const double dt = option.maturity / n;
  const bool american = option.exercise == Exercise::kAmerican;
  const bool call = option.option_type == OptionType::kCall;
  const auto size = static_cast<std::size_t>(m) + 1;

  // Grid coordinates and asset prices.
  const double x_lo = grid.log_space ? std::log(grid.s_min) : grid.s_min;
  const double x_hi = grid.log_space ? std::log(grid.s_max) : grid.s_max;
  const double h = (x_hi - x_lo) / m;
  std::vector<double> spot(size);
  for (std::size_t j = 0; j < size; ++j) {
    const double x = x_lo + h * static_cast<double>(j);
    spot[j] = grid.log_space ? std::exp(x) : x;
  }
  std::vector<Stencil> stencil(size);
  for (std::size_t j = 0; j < size; ++j) {
    double diffusion;
    double drift;
    if (grid.log_space) {
      diffusion = 0.5 * sigma * sigma;
      drift = r - q - 0.5 * sigma * sigma;
    } else {
      diffusion = 0.5 * sigma * sigma * spot[j] * spot[j];
      drift = (r - q) * spot[j];
    }
    stencil[j] = {diffusion / (h * h) - drift / (2.0 * h),
                  -2.0 * diffusion / (h * h),
                  diffusion / (h * h) + drift / (2.0 * h)};
  }

  std::vector<double> intrinsic(size);
  for (std::size_t j = 0; j < size; ++j) {
    intrinsic[j] = IntrinsicValue(option, spot[j]);
  }
  // Dirichlet boundaries at time to expiry tau.
  auto lower_boundary = [&](double tau) {
    if (call) return 0.0;
    if (american) return option.strike - spot.front();
    return std::max(
        option.strike * std::exp(-r * tau) - spot.front() * std::exp(-q * tau),
        0.0);
  };
  auto upper_boundary = [&](double tau) {
    if (!call) return 0.0;
    if (american) return spot.back() - option.strike;
    return std::max(
        spot.back() * std::exp(-q * tau) - option.strike * std::exp(-r * tau),
        0.0);
  };

  std::vector<double> f = intrinsic;
  const double theta = scheme == FdScheme::kCrankNicolson ? 0.5 : 1.0;
  const std::size_t inner = size - 2;
  for (int step = 1; step <= n; ++step) {
    const double tau = dt * step;
    std::vector<double> next(size);
    next.front() = lower_boundary(tau);
    next.back() = upper_boundary(tau);
    if (scheme == FdScheme::kExplicit) {
      // f_{i,j} (1 + r dt) = f_{i+1,j} + dt (a f_{j-1} + b f_j + c f_{j+1}).
      for (std::size_t j = 1; j + 1 < size; ++j) {
        const Stencil& s = stencil[j];
        next[j] = (f[j] + dt * (s.a * f[j - 1] + s.b * f[j] + s.c * f[j + 1])) /
                  (1.0 + r * dt);
        if (american) next[j] = std::max(next[j], intrinsic[j]);
      }
    } else {
      // (I - theta dt L) f_i = (I + (1 - theta) dt L) f_{i+1}, L including
      // the -r f term; boundary values move to the right-hand side.
      std::vector<double> lo(inner);
      std::vector<double> mid(inner);
      std::vector<double> hi(inner);
      std::vector<double> rhs(inner);
      for (std::size_t k = 0; k < inner; ++k) {
        const std::size_t j = k + 1;
        const Stencil& s = stencil[j];
        lo[k] = -theta * dt * s.a;
        mid[k] = 1.0 - theta * dt * (s.b - r);
        hi[k] = -theta * dt * s.c;
        const double explicit_part =
            s.a * f[j - 1] + (s.b - r) * f[j] + s.c * f[j + 1];
        rhs[k] = f[j] + (1.0 - theta) * dt * explicit_part;
      }
      rhs.front() -= lo.front() * next.front();
      rhs.back() -= hi.back() * next.back();
      lo.front() = 0.0;
      hi.back() = 0.0;
      std::vector<double> x;
      if (american && solver == AmericanSolver::kProjection) {
        x = SolveTridiagonal(lo, mid, hi, rhs);
        for (std::size_t k = 0; k < x.size(); ++k) {
          x[k] = std::max(x[k], intrinsic[k + 1]);
        }
      } else if (american) {
        const std::vector<double> floor(intrinsic.begin() + 1,
                                        intrinsic.end() - 1);
        const bool m_matrix = IsDominantMMatrix(lo, mid, hi);
        if (solver == AmericanSolver::kBrennanSchwartz && m_matrix) {
          x = BrennanSchwartz(lo, mid, hi, rhs, floor, call);
        } else {
          // PSOR starts from the Brennan-Schwartz solution when it applies,
          // which leaves it one or two sweeps to confirm convergence, and
          // from the previous time step otherwise.
          if (m_matrix) {
            x = BrennanSchwartz(lo, mid, hi, rhs, floor, call);
          } else {
            x.assign(f.begin() + 1, f.end() - 1);
          }
          Psor(lo, mid, hi, rhs, floor, x);
        }
      } else {
        x = SolveTridiagonal(lo, mid, hi, rhs);
      }
      std::copy(x.begin(), x.end(), next.begin() + 1);
    }
    f = std::move(next);
  }

  // Linear interpolation at S0 in the grid coordinate.
  const double x0 = grid.log_space ? std::log(market.spot) : market.spot;
  const double pos = (x0 - x_lo) / h;
  const auto j = static_cast<std::size_t>(
      std::clamp(std::floor(pos), 0.0, static_cast<double>(m - 1)));
  const double w = pos - static_cast<double>(j);
  return (1.0 - w) * f[j] + w * f[j + 1];
}

}  // namespace ofod::pricers
