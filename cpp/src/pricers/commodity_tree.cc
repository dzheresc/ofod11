// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/commodity_tree.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace ofod::pricers {
namespace {

std::size_t Index(int j, int width) {
  const int i = j + width;
  return static_cast<std::size_t>(i);
}

}  // namespace

std::optional<CommodityTree> CommodityTree::Build(
    double spot, std::span<const double> futures, double a, double sigma,
    double dt) {
  if (!(spot > 0.0) || futures.empty() || !(a > 0.0) || !(sigma > 0.0) ||
      !(dt > 0.0)) {
    return std::nullopt;
  }
  for (double f : futures) {
    if (!(f > 0.0)) return std::nullopt;
  }
  CommodityTree tree;
  tree.steps_ = static_cast<int>(futures.size());
  tree.dt_ = dt;
  tree.dx_ = sigma * std::sqrt(3.0 * dt);
  tree.jmax_ =
      std::min(static_cast<int>(std::floor(0.184 / (a * dt))) + 1, tree.steps_);
  const double m = -a * dt;
  for (int j = -tree.jmax_; j <= tree.jmax_; ++j) {
    tree.branches_.push_back(HullBranch(j, tree.jmax_, m));
  }
  tree.alphas_.push_back(std::log(spot));
  tree.p_.push_back({1.0});
  for (int i = 0; i < tree.steps_; ++i) {
    const int w = tree.Width(i);
    const int w_next = tree.Width(i + 1);
    const std::vector<double>& p = tree.p_.back();
    std::vector<double> next(Index(w_next, w_next) + 1, 0.0);
    for (int j = -w; j <= w; ++j) {
      const Branch& b = tree.BranchAt(j);
      const double here = p[Index(j, w)];
      next[Index(b.middle + 1, w_next)] += here * b.up;
      next[Index(b.middle, w_next)] += here * b.mid;
      next[Index(b.middle - 1, w_next)] += here * b.down;
    }
    // Choose alpha so that sum_j p_j exp(alpha + j dx) = F(0, (i + 1) dt).
    double sum = 0.0;
    for (int j = -w_next; j <= w_next; ++j) {
      sum += next[Index(j, w_next)] * std::exp(j * tree.dx_);
    }
    tree.alphas_.push_back(std::log(futures[static_cast<std::size_t>(i)]) -
                           std::log(sum));
    tree.p_.push_back(std::move(next));
  }
  return tree;
}

int CommodityTree::Width(int step) const { return std::min(step, jmax_); }

double CommodityTree::Price(int step, int j) const {
  return std::exp(alphas_[static_cast<std::size_t>(step)] + j * dx_);
}

const Branch& CommodityTree::BranchAt(int j) const {
  return branches_[Index(j, jmax_)];
}

std::span<const double> CommodityTree::Probabilities(int step) const {
  return p_[static_cast<std::size_t>(step)];
}

std::vector<double> CommodityTree::Rollback(std::span<const double> next,
                                            int step, double rate) const {
  const int w = Width(step);
  const int w_next = Width(step + 1);
  const double discount = std::exp(-rate * dt_);
  std::vector<double> out(Index(w, w) + 1);
  for (int j = -w; j <= w; ++j) {
    const Branch& b = BranchAt(j);
    out[Index(j, w)] = discount * (b.up * next[Index(b.middle + 1, w_next)] +
                                   b.mid * next[Index(b.middle, w_next)] +
                                   b.down * next[Index(b.middle - 1, w_next)]);
  }
  return out;
}

std::optional<double> CommodityTreeOption(const CommodityTree& tree,
                                          instruments::OptionType type,
                                          instruments::Exercise exercise,
                                          double strike, int expiry_step,
                                          double rate) {
  if (expiry_step < 0 || expiry_step > tree.steps()) return std::nullopt;
  const double sign = type == instruments::OptionType::kCall ? 1.0 : -1.0;
  auto payoff = [&](int step, int j) {
    return std::max(sign * (tree.Price(step, j) - strike), 0.0);
  };
  const int w = tree.Width(expiry_step);
  std::vector<double> values;
  values.reserve(Index(w, w) + 1);
  for (int j = -w; j <= w; ++j) values.push_back(payoff(expiry_step, j));
  for (int i = expiry_step - 1; i >= 0; --i) {
    values = tree.Rollback(values, i, rate);
    if (exercise == instruments::Exercise::kAmerican) {
      const int wi = tree.Width(i);
      for (int j = -wi; j <= wi; ++j) {
        values[Index(j, wi)] = std::max(values[Index(j, wi)], payoff(i, j));
      }
    }
  }
  return values[0];
}

}  // namespace ofod::pricers
