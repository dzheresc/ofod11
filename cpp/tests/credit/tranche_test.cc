// Copyright 2026 Denis Zhereshchin

#include "ofod/credit/tranche.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>

namespace ofod::credit {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Waterfall allocates losses bottom up", "[credit][ch08]") {
  // Figure 8.1-style ABS: equity 0-5%, mezzanine 5-20%, senior 20-100%.
  const std::vector<Tranche> abs = {{0.0, 0.05}, {0.05, 0.20}, {0.20, 1.0}};
  const auto at10 = WaterfallLosses(abs, 0.10);
  CHECK(at10[0] == 1.0);
  CHECK_THAT(at10[1], WithinAbs(1.0 / 3.0, 1e-15));
  CHECK(at10[2] == 0.0);
  const auto at25 = WaterfallLosses(abs, 0.25);
  CHECK_THAT(at25[2], WithinAbs(0.0625, 1e-15));
}

TEST_CASE("Tranche loss conserves pool principal", "[credit][ch08]") {
  const std::vector<Tranche> abs = {{0.0, 0.05}, {0.05, 0.20}, {0.20, 1.0}};
  for (double pool : {0.0, 0.03, 0.12, 0.5, 1.0}) {
    const auto losses = WaterfallLosses(abs, pool);
    double total = 0.0;
    for (std::size_t i = 0; i < abs.size(); ++i) {
      total += losses[i] * (abs[i].detachment - abs[i].attachment);
    }
    CHECK_THAT(total, WithinAbs(pool, 1e-15));
  }
}

}  // namespace
}  // namespace ofod::credit
