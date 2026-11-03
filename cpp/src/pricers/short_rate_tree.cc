// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/short_rate_tree.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "ofod/math/roots.h"

namespace ofod::pricers {
namespace {

std::size_t Index(int j, int width) {
  const int i = j + width;
  return static_cast<std::size_t>(i);
}

}  // namespace

Branch HullBranch(int j, int jmax, double m) {
  const double jm = j * m;
  const double jm2 = jm * jm;
  if (j == jmax && jmax > 0) {  // Branching (c): j, j - 1, j - 2.
    return {.middle = j - 1,
            .up = 7.0 / 6.0 + 0.5 * (jm2 + 3.0 * jm),
            .mid = -1.0 / 3.0 - jm2 - 2.0 * jm,
            .down = 1.0 / 6.0 + 0.5 * (jm2 + jm)};
  }
  if (j == -jmax && jmax > 0) {  // Branching (b): j + 2, j + 1, j.
    return {.middle = j + 1,
            .up = 1.0 / 6.0 + 0.5 * (jm2 - jm),
            .mid = -1.0 / 3.0 - jm2 + 2.0 * jm,
            .down = 7.0 / 6.0 + 0.5 * (jm2 - 3.0 * jm)};
  }
  return {.middle = j,
          .up = 1.0 / 6.0 + 0.5 * (jm2 + jm),
          .mid = 2.0 / 3.0 - jm2,
          .down = 1.0 / 6.0 + 0.5 * (jm2 - jm)};
}

std::optional<ShortRateTree> ShortRateTree::Build(
    const curves::ZeroCurve& curve, double a, double sigma, double dt,
    int steps, ShortRateModel model) {
  if (!(dt > 0.0) || !(sigma > 0.0) || steps <= 0 || a < 0.0) {
    return std::nullopt;
  }
  ShortRateTree tree;
  tree.steps_ = steps;
  tree.dt_ = dt;
  tree.model_ = model;
  tree.dx_ = sigma * std::sqrt(3.0 * dt);
  tree.jmax_ =
      a > 0.0
          ? std::min(static_cast<int>(std::floor(0.184 / (a * dt))) + 1, steps)
          : steps;
  const double m = -a * dt;
  for (int j = -tree.jmax_; j <= tree.jmax_; ++j) {
    tree.branches_.push_back(HullBranch(j, tree.jmax_, m));
    tree.node_discount_.push_back(std::exp(-j * tree.dx_ * dt));
    if (model == ShortRateModel::kBlackKarasinski) {
      tree.node_growth_.push_back(std::exp(j * tree.dx_));
    }
  }
  tree.q_.push_back({1.0});
  for (int i = 0; i < steps; ++i) {
    const int w = tree.Width(i);
    const std::vector<double>& q = tree.q_.back();
    const double target = curve.DiscountFactor((i + 1) * dt);
    double alpha = 0.0;
    if (model == ShortRateModel::kHullWhite) {
      double sum = 0.0;
      for (int j = -w; j <= w; ++j) {
        sum += q[Index(j, w)] * tree.node_discount_[Index(j, tree.jmax_)];
      }
      alpha = (std::log(sum) - std::log(target)) / dt;
    } else {
      // r = e^alpha e^{j dx}: with e^{j dx} tabulated, each trial alpha
      // costs one exp() per node instead of two.
      auto error = [&](double x) {
        const double scale = std::exp(x) * dt;
        double sum = 0.0;
        for (int j = -w; j <= w; ++j) {
          sum += q[Index(j, w)] *
                 std::exp(-scale * tree.node_growth_[Index(j, tree.jmax_)]);
        }
        return sum - target;
      };
      // Newton-Raphson from the previous step's alpha, as Hull suggests:
      // the value and the slope share each node's exp(), and alpha moves
      // little between steps, so it converges in a few iterations where
      // Brent on [-30, 3] needed dozens. Brent remains the fallback.
      std::optional<double> root;
      double x = tree.alphas_.empty() ? std::log(curve.ForwardRate(0.0, dt))
                                      : tree.alphas_.back();
      for (int it = 0; it < 50 && std::isfinite(x); ++it) {
        const double scale = std::exp(x) * dt;
        double value = -target;
        double slope = 0.0;
        for (int j = -w; j <= w; ++j) {
          const double rdt = scale * tree.node_growth_[Index(j, tree.jmax_)];
          const double term = q[Index(j, w)] * std::exp(-rdt);
          value += term;
          slope -= term * rdt;
        }
        if (!(slope < 0.0)) break;
        const double step = value / slope;
        x -= step;
        // Rounding in the sum limits alpha to about 1e-13, so stop at
        // Brent's tolerance.
        if (std::abs(step) < 1e-12) {
          root = x;
          break;
        }
      }
      if (!root || !(*root > -30.0 && *root < 3.0)) {
        root = math::Brent(error, -30.0, 3.0);
      }
      if (!root) return std::nullopt;
      alpha = *root;
      // Store this step's node discount factors so that neither the
      // forward induction below nor any rollback calls exp() again.
      const double scale = std::exp(alpha) * dt;
      std::vector<double> discount(Index(w, w) + 1);
      for (int j = -w; j <= w; ++j) {
        discount[Index(j, w)] =
            std::exp(-scale * tree.node_growth_[Index(j, tree.jmax_)]);
      }
      tree.bk_discount_.push_back(std::move(discount));
    }
    tree.alphas_.push_back(alpha);
    tree.step_discount_.push_back(std::exp(-alpha * dt));
    const int w_next = tree.Width(i + 1);
    std::vector<double> next(Index(w_next, w_next) + 1, 0.0);
    for (int j = -w; j <= w; ++j) {
      const Branch& b = tree.BranchAt(j);
      const double flow = q[Index(j, w)] * tree.Discount(i, j);
      next[Index(b.middle + 1, w_next)] += flow * b.up;
      next[Index(b.middle, w_next)] += flow * b.mid;
      next[Index(b.middle - 1, w_next)] += flow * b.down;
    }
    tree.q_.push_back(std::move(next));
  }
  return tree;
}

int ShortRateTree::Width(int step) const { return std::min(step, jmax_); }

double ShortRateTree::Rate(int step, int j) const {
  const double x = alphas_[static_cast<std::size_t>(step)] + j * dx_;
  return model_ == ShortRateModel::kHullWhite ? x : std::exp(x);
}

double ShortRateTree::Discount(int step, int j) const {
  if (model_ == ShortRateModel::kHullWhite) {
    return step_discount_[static_cast<std::size_t>(step)] *
           node_discount_[Index(j, jmax_)];
  }
  return bk_discount_[static_cast<std::size_t>(step)][Index(j, Width(step))];
}

const Branch& ShortRateTree::BranchAt(int j) const {
  return branches_[Index(j, jmax_)];
}

std::span<const double> ShortRateTree::ArrowDebreu(int step) const {
  return q_[static_cast<std::size_t>(step)];
}

std::vector<double> ShortRateTree::Rollback(std::span<const double> next,
                                            int step) const {
  const int w = Width(step);
  const int w_next = Width(step + 1);
  std::vector<double> out(Index(w, w) + 1);
  for (int j = -w; j <= w; ++j) {
    const Branch& b = BranchAt(j);
    const double expected = b.up * next[Index(b.middle + 1, w_next)] +
                            b.mid * next[Index(b.middle, w_next)] +
                            b.down * next[Index(b.middle - 1, w_next)];
    out[Index(j, w)] = Discount(step, j) * expected;
  }
  return out;
}

std::vector<double> ShortRateTree::ZeroBondValues(int step,
                                                  int maturity_step) const {
  const int w = Width(maturity_step);
  std::vector<double> values(Index(w, w) + 1, 1.0);
  for (int s = maturity_step - 1; s >= step; --s) values = Rollback(values, s);
  return values;
}

std::optional<double> TreeBondOption(const ShortRateTree& tree, OptionType type,
                                     Exercise exercise, int expiry_step,
                                     std::span<const TreeCashFlow> bond,
                                     double strike) {
  if (bond.empty()) return std::nullopt;
  int last = 0;
  for (const TreeCashFlow& c : bond) {
    if (c.step < 0 || c.step > tree.steps()) return std::nullopt;
    last = std::max(last, c.step);
  }
  if (expiry_step < 0 || expiry_step > last) return std::nullopt;
  const double phi = type == OptionType::kCall ? 1.0 : -1.0;
  std::vector<double> value(Index(tree.Width(last), tree.Width(last)) + 1, 0.0);
  std::vector<double> option;
  for (int s = last; s >= 0; --s) {
    if (s < last) {
      value = tree.Rollback(value, s);
      if (!option.empty()) option = tree.Rollback(option, s);
    }
    if (s == expiry_step ||
        (s < expiry_step && exercise == Exercise::kAmerican)) {
      if (option.empty()) option.assign(value.size(), 0.0);
      for (std::size_t k = 0; k < value.size(); ++k) {
        option[k] = std::max(option[k], phi * (value[k] - strike));
      }
    }
    for (const TreeCashFlow& c : bond) {
      if (c.step != s) continue;
      for (double& v : value) v += c.amount;
    }
  }
  return option.front();
}

std::optional<double> TreeSwaption(const ShortRateTree& tree, OptionType type,
                                   double strike, int period_steps,
                                   int end_step,
                                   std::span<const int> exercise_steps) {
  if (period_steps <= 0 || end_step > tree.steps() || exercise_steps.empty()) {
    return std::nullopt;
  }
  for (int e : exercise_steps) {
    if (e < 0 || e >= end_step || (end_step - e) % period_steps != 0) {
      return std::nullopt;
    }
  }
  const double phi = type == OptionType::kCall ? 1.0 : -1.0;
  const double coupon = strike * period_steps * tree.dt();
  // Fixed leg including the final principal, per unit notional.
  std::vector<double> fixed(
      Index(tree.Width(end_step), tree.Width(end_step)) + 1, 0.0);
  std::vector<double> option;
  for (int s = end_step; s >= 0; --s) {
    if (s < end_step) {
      fixed = tree.Rollback(fixed, s);
      if (!option.empty()) option = tree.Rollback(option, s);
    }
    if (std::find(exercise_steps.begin(), exercise_steps.end(), s) !=
        exercise_steps.end()) {
      if (option.empty()) option.assign(fixed.size(), 0.0);
      for (std::size_t k = 0; k < fixed.size(); ++k) {
        option[k] = std::max(option[k], phi * (1.0 - fixed[k]));
      }
    }
    if ((end_step - s) % period_steps == 0) {
      const double cash = coupon + (s == end_step ? 1.0 : 0.0);
      for (double& v : fixed) v += cash;
    }
  }
  return option.empty() ? 0.0 : option.front();
}

}  // namespace ofod::pricers
