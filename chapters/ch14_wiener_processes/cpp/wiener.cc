// Copyright 2026 Denis Zhereshchin
//
// Chapter 14: the Table 14.1 GBM simulation, quadratic variation of a
// Wiener path, correlated normals, and a Monte Carlo check of Ito's lemma.

#include <cmath>
#include <format>
#include <iostream>
#include <vector>

#include "ofod/models/brownian.h"
#include "ofod/random/normal_generator.h"

int main() {
  namespace models = ofod::models;
  namespace random = ofod::random;

  const models::GeometricBrownianMotion gbm{.drift = 0.15, .volatility = 0.30};
  const std::vector<double> eps = {0.52, 1.44, -0.86, 1.46, -0.69};
  const auto path = gbm.EulerPath(100.0, 0.0192, eps);
  std::cout << "Table 14.1: S over five weeks:";
  for (double s : path) std::cout << std::format(" {:.2f}", s);
  std::cout << '\n';

  random::NormalGenerator gen(2026);
  const models::ArithmeticBrownianMotion w{.drift = 0.0, .volatility = 1.0};
  for (int n : {10, 1000, 100000}) {
    const auto wiener = w.Path(0.0, 1.0 / n, gen.Sample(n));
    std::cout << std::format(
        "Wiener path on [0, 1], {:>6} steps: quadratic variation {:.4f}\n", n,
        models::QuadraticVariation(wiener));
  }

  const auto lower = random::Cholesky({{1.0, 0.6}, {0.6, 1.0}});
  if (!lower) return 1;
  const std::vector<double> z = {1.0, 2.0};
  const auto e = random::Correlate(*lower, z);
  std::cout << std::format("rho = 0.6: (1, 2) -> ({:.2f}, {:.2f})\n", e[0],
                           e[1]);

  const models::GeometricBrownianMotion stock{.drift = 0.12, .volatility = 0.4};
  double sum = 0.0;
  const int paths = 5000;
  for (int i = 0; i < paths; ++i) {
    sum += std::log(stock.EulerPath(1.0, 1.0 / 250, gen.Sample(250)).back());
  }
  std::cout << std::format(
      "Ito: mean ln(S_1/S_0) = {:.4f} (s.e. {:.4f}), mu - sigma^2/2 = "
      "{:.4f}\n",
      sum / paths, 0.4 / std::sqrt(paths), 0.12 - 0.5 * 0.4 * 0.4);
  return 0;
}
