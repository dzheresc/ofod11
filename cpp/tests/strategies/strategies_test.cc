// Copyright 2026 Denis Zhereshchin

#include "ofod/strategies/strategies.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>

namespace ofod::strategies {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Box spread pays the strike difference everywhere",
          "[strategies][ch12]") {
  const auto box = BoxSpread(50.0, 6.0, 1.0, 60.0, 1.5, 6.0);
  for (double s : {30.0, 55.0, 80.0}) CHECK(box.PayoffAt(s) == 10.0);
}

TEST_CASE("Strip and strap weight the two sides", "[strategies][ch12]") {
  CHECK(Strip(50.0, 3.0, 2.0).PayoffAt(40.0) == 20.0);
  CHECK(Strap(50.0, 3.0, 2.0).PayoffAt(60.0) == 20.0);
  CHECK(Strangle(45.0, 1.0, 55.0, 1.0).PayoffAt(50.0) == 0.0);
}

TEST_CASE("Replicating a piecewise-linear payoff", "[strategies][ch12]") {
  // A bull spread 40/50 plus 5 in cash: slopes 0, 1, 0.
  const std::vector<double> slopes = {0.0, 1.0, 0.0};
  const std::vector<double> kinks = {40.0, 50.0};
  const auto p = ReplicatePayoff(5.0, slopes, kinks);
  REQUIRE(p.has_value());
  CHECK(p->PayoffAt(30.0) == 5.0);
  CHECK(p->PayoffAt(45.0) == 10.0);
  CHECK(p->PayoffAt(70.0) == 15.0);
  CHECK_FALSE(ReplicatePayoff(0.0, slopes, std::vector<double>{50.0, 40.0}));
  CHECK_FALSE(ReplicatePayoff(0.0, kinks, kinks));
}

TEST_CASE("Covered call and protective put", "[strategies][ch12]") {
  CHECK(CoveredCall(40.0, 45.0, 2.0).ProfitAt(50.0) == 7.0);
  CHECK(ProtectivePut(40.0, 35.0, 1.0).ProfitAt(20.0) == -6.0);
  CHECK_THAT(PrincipalProtectedBudget(1000.0, 0.06, 3.0),
             WithinAbs(164.73, 0.005));
}

}  // namespace
}  // namespace ofod::strategies
