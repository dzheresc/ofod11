// Copyright 2026 Denis Zhereshchin
//
// Reproduces the book's numbers stored in data/golden/*.json.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ofod/core/compounding.h"
#include "ofod/curves/bond_bootstrap.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/bond.h"
#include "ofod/pricers/bond_pricer.h"
#include "ofod/pricers/forward_pricer.h"

namespace ofod {
namespace {

using Catch::Matchers::WithinAbs;
using nlohmann::json;

json LoadChapter(int chapter) {
  const std::string path = std::string(OFOD_GOLDEN_DIR) + "/ch" +
                           (chapter < 10 ? "0" : "") + std::to_string(chapter) +
                           ".json";
  std::ifstream in(path);
  REQUIRE(in.good());
  return json::parse(in);
}

Compounding FromFrequency(int frequency) {
  return frequency == 0 ? Compounding::Continuous()
                        : Compounding::Periodic(frequency);
}

curves::ZeroCurve ParseCurve(const json& j) {
  auto curve = curves::ZeroCurve::Create(j["times"].get<std::vector<double>>(),
                                         j["rates"].get<std::vector<double>>());
  REQUIRE(curve.has_value());
  return *curve;
}

instruments::Bond ParseBond(const json& j) {
  return {.face = j["face"].get<double>(),
          .coupon_rate = j["coupon_rate"].get<double>(),
          .maturity = j["maturity"].get<double>(),
          .frequency = j["frequency"].get<int>()};
}

// Returns the value the library computes for one scalar case.
double EvaluateScalar(const std::string& kind, const json& in) {
  if (kind == "growth_factor") {
    return in["principal"].get<double>() *
           GrowthFactor(in["rate"], in["t"], FromFrequency(in["frequency"]));
  }
  if (kind == "equivalent_rate") {
    return EquivalentRate(in["rate"], FromFrequency(in["from_frequency"]),
                          FromFrequency(in["to_frequency"]));
  }
  if (kind == "bond_price") {
    return pricers::BondPrice(ParseBond(in["bond"]), ParseCurve(in["curve"]));
  }
  if (kind == "bond_yield") {
    const auto y = pricers::BondYield(ParseBond(in["bond"]), in["price"]);
    REQUIRE(y.has_value());
    return *y;
  }
  if (kind == "par_yield") {
    return pricers::ParYield(in["maturity"], in["frequency"],
                             ParseCurve(in["curve"]));
  }
  if (kind == "bond_price_from_yield") {
    return pricers::BondPriceFromYield(ParseBond(in["bond"]), in["yield"]);
  }
  if (kind == "macaulay_duration") {
    return pricers::MacaulayDuration(ParseBond(in["bond"]), in["yield"]);
  }
  if (kind == "modified_duration") {
    return pricers::ModifiedDuration(ParseBond(in["bond"]), in["yield"],
                                     FromFrequency(in["frequency"]));
  }
  if (kind == "convexity") {
    return pricers::Convexity(ParseBond(in["bond"]), in["yield"]);
  }
  if (kind == "forward_price") {
    return pricers::ForwardPrice(in["spot"], in["rate"], in["t"]);
  }
  if (kind == "forward_price_with_income") {
    std::vector<instruments::CashFlow> income;
    for (const auto& flow : in["income"]) {
      income.push_back({flow["time"], flow["amount"]});
    }
    const auto curve = ParseCurve(in["curve"]);
    const double t = in["t"];
    return pricers::ForwardPriceWithIncome(
        in["spot"], pricers::PresentValue(income, curve), curve.ZeroRate(t), t);
  }
  if (kind == "forward_price_with_yield") {
    const double q =
        EquivalentRate(in["yield"], FromFrequency(in["yield_frequency"]),
                       Compounding::Continuous());
    return pricers::ForwardPriceWithYield(in["spot"], in["rate"], q, in["t"]);
  }
  if (kind == "forward_value") {
    const double f = pricers::ForwardPrice(in["spot"], in["rate"], in["t"]);
    return pricers::ForwardContractValue(f, in["delivery_price"], in["rate"],
                                         in["t"], instruments::Side::kLong);
  }
  FAIL("unknown golden case kind: " << kind);
  return 0.0;
}

// Returns the values the library computes for one vector case.
std::vector<double> EvaluateVector(const std::string& kind, const json& in) {
  if (kind == "bootstrap") {
    std::vector<curves::BondQuote> quotes;
    for (const auto& q : in["quotes"]) {
      quotes.push_back({ParseBond(q["bond"]), q["price"]});
    }
    const auto curve = curves::BootstrapFromBonds(quotes);
    REQUIRE(curve.has_value());
    return {curve->zero_rates().begin(), curve->zero_rates().end()};
  }
  if (kind == "forward_rates") {
    const auto curve = ParseCurve(in["curve"]);
    std::vector<double> forwards;
    for (std::size_t i = 1; i < curve.times().size(); ++i) {
      forwards.push_back(
          curve.ForwardRate(curve.times()[i - 1], curve.times()[i]));
    }
    return forwards;
  }
  FAIL("unknown golden case kind: " << kind);
  return {};
}

void RunChapter(int chapter) {
  const json doc = LoadChapter(chapter);
  for (const auto& c : doc["cases"]) {
    const std::string id = c["id"];
    const std::string kind = c["kind"];
    const double tol = c["tol"];
    INFO(id << " (" << c["ref"].get<std::string>() << ")");
    if (c["expected"].is_array()) {
      const auto expected = c["expected"].get<std::vector<double>>();
      const auto actual = EvaluateVector(kind, c["inputs"]);
      REQUIRE(actual.size() == expected.size());
      for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK_THAT(actual[i], WithinAbs(expected[i], tol));
      }
    } else {
      CHECK_THAT(EvaluateScalar(kind, c["inputs"]),
                 WithinAbs(c["expected"].get<double>(), tol));
    }
  }
}

TEST_CASE("Golden: Chapter 4 interest rates", "[golden][ch04]") {
  RunChapter(4);
}

TEST_CASE("Golden: Chapter 5 forward prices", "[golden][ch05]") {
  RunChapter(5);
}

}  // namespace
}  // namespace ofod
