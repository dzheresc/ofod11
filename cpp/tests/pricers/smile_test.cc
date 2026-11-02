// Copyright 2026 Denis Zhereshchin
//
// Volatility surfaces, smile tools (Ch 20) and employee stock options
// (Ch 16).

#include "ofod/pricers/smile.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "ofod/curves/vol_surface.h"
#include "ofod/greeks/greeks.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/eso.h"

namespace ofod::pricers {
namespace {

using Catch::Matchers::WithinAbs;
using curves::VolInterpolation;
using curves::VolSurface;
using instruments::OptionType;

std::optional<VolSurface> Surface(
    VolInterpolation i = VolInterpolation::kTotalVariance) {
  return VolSurface::Create({0.5, 1.0}, {90.0, 100.0, 110.0},
                            {{0.25, 0.20, 0.22}, {0.24, 0.21, 0.22}}, i);
}

TEST_CASE("VolSurface validates its grid", "[curves][ch20]") {
  CHECK(Surface().has_value());
  CHECK_FALSE(VolSurface::Create({1.0, 0.5}, {100.0}, {{0.2}, {0.2}}));
  CHECK_FALSE(VolSurface::Create({0.0}, {100.0}, {{0.2}}));
  CHECK_FALSE(VolSurface::Create({1.0}, {90.0, 100.0}, {{0.2}}));
  CHECK_FALSE(VolSurface::Create({1.0}, {100.0}, {{0.0}}));
  CHECK_FALSE(VolSurface::Create({}, {}, {}));
}

TEST_CASE("VolSurface reproduces nodes and extrapolates flat",
          "[curves][ch20]") {
  for (auto interp :
       {VolInterpolation::kTotalVariance, VolInterpolation::kLinearVol}) {
    const auto s = Surface(interp);
    REQUIRE(s.has_value());
    CHECK_THAT(s->Volatility(90.0, 0.5), WithinAbs(0.25, 1e-15));
    CHECK_THAT(s->Volatility(100.0, 1.0), WithinAbs(0.21, 1e-15));
    CHECK_THAT(s->Volatility(95.0, 0.5), WithinAbs(0.225, 1e-15));
    CHECK_THAT(s->Volatility(50.0, 0.1), WithinAbs(0.25, 1e-15));
    CHECK_THAT(s->Volatility(200.0, 3.0), WithinAbs(0.22, 1e-15));
  }
}

TEST_CASE("Total variance interpolation is linear in sigma^2 T",
          "[curves][ch20]") {
  const auto s = Surface();
  REQUIRE(s.has_value());
  const double v = s->Volatility(100.0, 0.75);
  const double expected = 0.5 * (0.20 * 0.20 * 0.5 + 0.21 * 0.21 * 1.0);
  CHECK_THAT(v * v * 0.75, WithinAbs(expected, 1e-15));
}

TEST_CASE("StrikeFromDelta inverts BSM delta", "[smile][ch20]") {
  const greeks::OptionInputs in{.spot = 1.2,
                                .rate = 0.03,
                                .yield = 0.01,
                                .volatility = 0.12,
                                .maturity = 0.5};
  for (auto [type, delta] :
       {std::pair{OptionType::kCall, 0.25}, std::pair{OptionType::kCall, 0.5},
        std::pair{OptionType::kPut, -0.25}}) {
    const auto k = StrikeFromDelta(type, delta, in.spot, in.rate, in.yield,
                                   in.volatility, in.maturity);
    REQUIRE(k.has_value());
    CHECK_THAT(greeks::BsmGreeks(type, *k, in).delta, WithinAbs(delta, 1e-12));
  }
  CHECK_FALSE(
      StrikeFromDelta(OptionType::kCall, 1.0, 1.2, 0.03, 0.01, 0.12, 0.5));
  CHECK_FALSE(
      StrikeFromDelta(OptionType::kPut, 0.1, 1.2, 0.03, 0.01, 0.12, 0.5));
}

TEST_CASE("Implied density from a flat smile is lognormal", "[smile][ch20]") {
  auto call = [](double k) {
    return BsmPrice(OptionType::kCall, 100.0, k, 0.05, 0.0, 0.2, 1.0);
  };
  const double h = 0.5;
  double mass = 0.0;
  for (double k = 20.0; k < 400.0; k += h) {
    const double g =
        ImpliedDensity(call(k - h), call(k), call(k + h), h, 0.05, 1.0);
    CHECK_THAT(
        g, WithinAbs(LognormalDensity(100.0, k, 0.05, 0.0, 0.2, 1.0), 1e-5));
    mass += g * h;
  }
  CHECK_THAT(mass, WithinAbs(1.0, 1e-4));
}

TEST_CASE("A binary jump satisfies put-call parity and gives a frown",
          "[smile][ch20]") {
  const double c =
      BinaryJumpPrice(OptionType::kCall, 50.0, 0.02, 0.1, 58.0, 42.0, 0.5);
  const double p =
      BinaryJumpPrice(OptionType::kPut, 50.0, 0.02, 0.1, 58.0, 42.0, 0.5);
  const double forward = std::exp(-0.02 * 0.1) * 50.0;
  CHECK_THAT(c - p, WithinAbs(forward - 50.0 * std::exp(-0.02 * 0.1), 1e-12));
  auto iv = [](double k) {
    const double price =
        BinaryJumpPrice(OptionType::kCall, k, 0.0, 0.1, 58.0, 42.0, 0.5);
    return *ImpliedVolatility(OptionType::kCall, price, 50.0, k, 0.0, 0.0, 0.1);
  };
  CHECK(iv(50.0) > iv(44.0));
  CHECK(iv(50.0) > iv(56.0));
}

TEST_CASE("Minimum variance delta reduces to BSM delta when a=b=c=0",
          "[smile][ch20]") {
  CHECK(MinimumVarianceDelta(0.6, 10.0, 50.0, 0.5, 0.0, 0.0, 0.0) == 0.6);
  CHECK(MinimumVarianceDelta(0.6, 10.0, 50.0, 0.5, -0.3, 0.0, 0.0) < 0.6);
}

const TreeMarket kEsoMarket{
    .spot = 40.0, .rate = 0.05, .yield = 0.0, .volatility = 0.3};

TEST_CASE("ESO without frictions is a European call on the same tree",
          "[eso][ch16]") {
  const auto eso = PriceEmployeeStockOption({.strike = 40.0,
                                             .maturity = 5.0,
                                             .vesting = 0.0,
                                             .exercise_multiple = 1e12,
                                             .exit_rate = 0.0},
                                            kEsoMarket, 200);
  const auto tree = PriceBinomial(
      {OptionType::kCall, instruments::Exercise::kEuropean, 40.0, 5.0},
      kEsoMarket, 200, TreeScheme::kCoxRossRubinstein);
  REQUIRE(eso.has_value());
  REQUIRE(tree.has_value());
  CHECK_THAT(*eso, WithinAbs(tree->price, 1e-10));
}

TEST_CASE("ESO value falls with exits and with early exercise", "[eso][ch16]") {
  EsoTerms terms{.strike = 40.0,
                 .maturity = 10.0,
                 .vesting = 3.0,
                 .exercise_multiple = 2.0,
                 .exit_rate = 0.03};
  auto value = [&](const EsoTerms& t) {
    return *PriceEmployeeStockOption(t, kEsoMarket, 400);
  };
  const double base = value(terms);
  EsoTerms more_exits = terms;
  more_exits.exit_rate = 0.10;
  EsoTerms earlier = terms;
  earlier.exercise_multiple = 1.3;
  CHECK(value(more_exits) < base);
  CHECK(value(earlier) < base);
  CHECK(base < BsmPrice(OptionType::kCall, 40.0, 40.0, 0.05, 0.0, 0.3, 10.0));
}

TEST_CASE("ESO rejects invalid terms", "[eso][ch16]") {
  const EsoTerms ok{.strike = 40.0,
                    .maturity = 10.0,
                    .vesting = 3.0,
                    .exercise_multiple = 1.5,
                    .exit_rate = 0.04};
  CHECK(PriceEmployeeStockOption(ok, kEsoMarket, 10).has_value());
  CHECK_FALSE(PriceEmployeeStockOption(ok, kEsoMarket, 0).has_value());
  EsoTerms bad = ok;
  bad.exercise_multiple = 0.9;
  CHECK_FALSE(PriceEmployeeStockOption(bad, kEsoMarket, 10).has_value());
  bad = ok;
  bad.exit_rate = -0.1;
  CHECK_FALSE(PriceEmployeeStockOption(bad, kEsoMarket, 10).has_value());
  bad = ok;
  bad.vesting = 11.0;
  CHECK_FALSE(PriceEmployeeStockOption(bad, kEsoMarket, 10).has_value());
}

}  // namespace
}  // namespace ofod::pricers
