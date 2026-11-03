// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/real_options.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <variant>
#include <vector>

namespace ofod::pricers {
namespace {

std::size_t Index(int j, int width) {
  const int i = j + width;
  return static_cast<std::size_t>(i);
}

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

struct Crr {
  double up;
  double down;
  double p;
  double discount;
};

std::optional<Crr> MakeCrr(const ProjectLattice& l) {
  if (!(l.value > 0.0) || !(l.volatility > 0.0) || !(l.maturity > 0.0) ||
      l.steps <= 0) {
    return std::nullopt;
  }
  const double dt = l.maturity / l.steps;
  const double up = std::exp(l.volatility * std::sqrt(dt));
  const double down = 1.0 / up;
  const double p = (std::exp((l.rate - l.payout) * dt) - down) / (up - down);
  if (!(p > 0.0 && p < 1.0)) return std::nullopt;
  return Crr{
      .up = up, .down = down, .p = p, .discount = std::exp(-l.rate * dt)};
}

// Rolls back an American claim on V with exercise value `exercise(V)`
// and terminal value `terminal(V)`. A holder of the project itself also
// receives its payout, `carry` V per step (carry = 0 for an option on V).
template <class Terminal, class Exercise>
double RollbackAmerican(const ProjectLattice& l, const Crr& c, double carry,
                        Terminal terminal, Exercise exercise) {
  const auto n = static_cast<std::size_t>(l.steps);
  std::vector<double> values(n + 1);
  for (std::size_t k = 0; k <= n; ++k) {
    const double v = l.value * std::pow(c.up, static_cast<double>(k)) *
                     std::pow(c.down, static_cast<double>(n - k));
    values[k] = terminal(v);
  }
  for (std::size_t i = n; i-- > 0;) {
    for (std::size_t k = 0; k <= i; ++k) {
      const double v = l.value * std::pow(c.up, static_cast<double>(k)) *
                       std::pow(c.down, static_cast<double>(i - k));
      const double hold =
          c.discount * (c.p * values[k + 1] + (1.0 - c.p) * values[k]) +
          carry * v;
      values[k] = std::max(hold, exercise(v));
    }
  }
  return values[0];
}

}  // namespace

double MarketPriceOfRisk(double correlation, double market_return, double rate,
                         double market_vol) {
  return correlation * (market_return - rate) / market_vol;
}

double RiskNeutralGrowth(double growth, double lambda, double volatility) {
  return growth - lambda * volatility;
}

std::optional<double> RiskNeutralPresentValue(std::span<const double> times,
                                              std::span<const double> expected,
                                              double lambda, double volatility,
                                              double rate) {
  if (times.size() != expected.size()) return std::nullopt;
  double pv = 0.0;
  for (std::size_t i = 0; i < times.size(); ++i) {
    pv += expected[i] * std::exp(-(lambda * volatility + rate) * times[i]);
  }
  return pv;
}

double ExerciseValue(const ProjectOption& option, double value) {
  return std::visit(
      Overloaded{
          [](const Abandon& a) { return a.salvage; },
          [value](const Expand& e) { return e.factor * value - e.cost; },
          [value](const Contract& c) { return c.factor * value + c.saving; }},
      option);
}

std::optional<double> ProjectWithOptions(
    const ProjectLattice& lattice, std::span<const ProjectOption> options) {
  const auto crr = MakeCrr(lattice);
  if (!crr) return std::nullopt;
  auto exercise = [options](double v) {
    double out = -std::numeric_limits<double>::infinity();
    for (const ProjectOption& o : options) {
      out = std::max(out, ExerciseValue(o, v));
    }
    return out;
  };
  auto terminal = [&](double v) { return std::max(v, exercise(v)); };
  // The payout received over a step makes the bare project worth exactly
  // V at every node, so with no options the result is V.
  const double dt = lattice.maturity / lattice.steps;
  const double carry = 1.0 - std::exp(-lattice.payout * dt);
  return RollbackAmerican(lattice, *crr, carry, terminal, exercise);
}

std::optional<double> DeferralOption(const ProjectLattice& lattice,
                                     double cost) {
  const auto crr = MakeCrr(lattice);
  if (!crr) return std::nullopt;
  auto payoff = [cost](double v) { return std::max(v - cost, 0.0); };
  return RollbackAmerican(lattice, *crr, 0.0, payoff, payoff);
}

std::optional<ProjectValuation> ValueExtractionProject(
    const CommodityTree& tree, const ExtractionProject& project) {
  const int n = tree.steps();
  auto cash_flow = [&](int step, int j) {
    return project.production * (tree.Price(step, j) - project.variable_cost) -
           project.fixed_cost;
  };
  std::vector<double> base;
  std::vector<double> abandon;
  std::vector<double> expand;
  const int wn = tree.Width(n);
  for (int j = -wn; j <= wn; ++j) {
    const double cf = cash_flow(n, j);
    base.push_back(cf);
    abandon.push_back(std::max(cf, project.abandon_value));
    expand.push_back(
        std::max(cf, project.expand_factor * cf - project.expand_cost));
  }
  for (int i = n - 1; i >= 0; --i) {
    base = tree.Rollback(base, i, project.rate);
    abandon = tree.Rollback(abandon, i, project.rate);
    expand = tree.Rollback(expand, i, project.rate);
    const int w = tree.Width(i);
    for (int j = -w; j <= w; ++j) {
      const std::size_t k = Index(j, w);
      const double cf = i > 0 ? cash_flow(i, j) : 0.0;
      base[k] += cf;
      abandon[k] = std::max(abandon[k] + cf, project.abandon_value);
      expand[k] = std::max(expand[k] + cf, project.expand_factor * base[k] -
                                               project.expand_cost);
    }
  }
  return ProjectValuation{
      .base = base[0], .with_abandon = abandon[0], .with_expand = expand[0]};
}

std::optional<BusinessValue> ValueBusiness(const BusinessSpec& spec, int paths,
                                           std::uint64_t seed) {
  if (!(spec.revenue > 0.0) || spec.volatility < 0.0 || !(spec.years > 0.0) ||
      spec.steps_per_year <= 0 || paths < 2) {
    return std::nullopt;
  }
  const int steps = static_cast<int>(
      std::lround(spec.years * static_cast<double>(spec.steps_per_year)));
  if (steps <= 0) return std::nullopt;
  const double dt = spec.years / steps;
  const double drift =
      (spec.growth - 0.5 * spec.volatility * spec.volatility) * dt;
  const double shock = spec.volatility * std::sqrt(dt);
  const double growth_cash = std::exp(spec.rate * dt);
  const double discount = std::exp(-spec.rate * spec.years);
  std::mt19937_64 engine(seed);
  std::normal_distribution<double> normal;
  double sum = 0.0;
  double sum_sq = 0.0;
  int bankrupt = 0;
  for (int p = 0; p < paths; ++p) {
    double revenue = spec.revenue;
    double cash = spec.cash;
    double last_flow = 0.0;
    bool alive = true;
    for (int i = 0; i < steps; ++i) {
      revenue *= std::exp(drift + shock * normal(engine));
      const double flow =
          (revenue * (1.0 - spec.variable_cost) - spec.fixed_cost) * dt;
      cash = cash * growth_cash + flow;
      last_flow = flow / dt;
      if (cash < 0.0) {
        alive = false;
        break;
      }
    }
    double value = 0.0;
    if (alive) {
      value =
          discount * (cash + spec.terminal_multiple * std::max(last_flow, 0.0));
    } else {
      ++bankrupt;
    }
    sum += value;
    sum_sq += value * value;
  }
  const double mean = sum / paths;
  const double variance =
      std::max((sum_sq / paths - mean * mean) * paths / (paths - 1), 0.0);
  return BusinessValue{
      .value = mean,
      .std_error = std::sqrt(variance / paths),
      .bankruptcy_probability = static_cast<double>(bankrupt) / paths};
}

}  // namespace ofod::pricers
