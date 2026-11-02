// Copyright 2026 Denis Zhereshchin

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "ofod/futures/margin.h"
#include "ofod/futures/rate_futures.h"
#include "ofod/futures/treasury.h"

namespace ofod::futures {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Short margin account gains when prices fall", "[futures][ch02]") {
  const MarginSpec spec{.initial_margin = 5000,
                        .maintenance_margin = 4000,
                        .contract_size = 1000,
                        .contracts = 1,
                        .side = instruments::Side::kShort};
  const std::vector<double> prices = {50.0, 49.0, 52.0};
  const auto days = SimulateMarginAccount(spec, prices);
  REQUIRE(days.size() == 3);
  CHECK(days[1].daily_gain == 1000.0);
  CHECK(days[2].balance == 3000.0);
  CHECK(days[2].margin_call == 2000.0);
  CHECK(SimulateMarginAccount(spec, {}).empty());
}

TEST_CASE("ParseThirtySeconds rejects bad quotes", "[futures][ch06]") {
  CHECK(ParseThirtySeconds("120-00") == 120.0);
  CHECK_FALSE(ParseThirtySeconds("95"));
  CHECK_FALSE(ParseThirtySeconds("95-32"));
  CHECK_FALSE(ParseThirtySeconds("95-1x"));
  CHECK_FALSE(ParseThirtySeconds("-16"));
}

TEST_CASE("Conversion factor of a 6% bond is 1", "[futures][ch06]") {
  for (double years : {15.0, 20.5, 29.5}) {
    CHECK_THAT(ConversionFactor(0.06, years), WithinAbs(1.0, 1e-12));
  }
  // Odd quarter: (3 + 100) / sqrt(1.03) - 1.5, slightly below par.
  const double odd = 103.0 / std::sqrt(1.03) - 1.5;
  CHECK_THAT(ConversionFactor(0.06, 15.25), WithinAbs(odd / 100.0, 1e-12));
}

TEST_CASE("CTD picks the lowest delivery cost", "[futures][ch06]") {
  const std::vector<DeliverableBond> bonds = {
      {99.50, 1.0382}, {143.50, 1.5188}, {119.75, 1.2615}};
  const auto ctd = FindCheapestToDeliver(93.25, bonds);
  REQUIRE(ctd.has_value());
  CHECK(ctd->index == 1);
  CHECK_FALSE(FindCheapestToDeliver(93.25, {}));
}

TEST_CASE("Three-month contract moves $25 per basis point", "[ch06]") {
  CHECK_THAT(ThreeMonthContractValue(95.53) - ThreeMonthContractValue(95.52),
             WithinAbs(25.0, 1e-9));
  CHECK_THAT(FuturesRateFromQuote(95.52), WithinAbs(0.0448, 1e-15));
}

}  // namespace
}  // namespace ofod::futures
