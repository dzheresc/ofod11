// Copyright 2026 Denis Zhereshchin
//
// Reproduces the book's numbers stored in data/golden/*.json.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "ofod/core/compounding.h"
#include "ofod/core/date.h"
#include "ofod/curves/bond_bootstrap.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/futures/margin.h"
#include "ofod/futures/rate_futures.h"
#include "ofod/futures/treasury.h"
#include "ofod/instruments/bond.h"
#include "ofod/instruments/cash_flow.h"
#include "ofod/instruments/option.h"
#include "ofod/instruments/portfolio.h"
#include "ofod/math/normal.h"
#include "ofod/models/brownian.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/bond_pricer.h"
#include "ofod/pricers/bounds.h"
#include "ofod/pricers/finite_difference.h"
#include "ofod/pricers/forward_pricer.h"
#include "ofod/pricers/monte_carlo.h"
#include "ofod/pricers/swap_pricer.h"
#include "ofod/random/normal_generator.h"
#include "ofod/random/sobol.h"
#include "ofod/risk/hedging.h"
#include "ofod/stats/descriptive.h"
#include "ofod/strategies/strategies.h"

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

futures::MarginSpec ParseMarginSpec(const json& j) {
  return {.initial_margin = j["initial_margin"],
          .maintenance_margin = j["maintenance_margin"],
          .contract_size = j["contract_size"],
          .contracts = j["contracts"],
          .side = j["side"] == "long" ? instruments::Side::kLong
                                      : instruments::Side::kShort};
}

Date ParseDate(const json& j) {
  return Date(j[0].get<int>(), j[1].get<unsigned>(), j[2].get<unsigned>());
}

instruments::InterestRateSwap ParseSwap(const json& j) {
  return {.notional = j["notional"],
          .fixed_rate = j["fixed_rate"],
          .frequency = j["frequency"],
          .payment_times = j["payment_times"].get<std::vector<double>>(),
          .next_floating_rate = j["next_floating_rate"]};
}

instruments::VanillaOption ParseOption(const json& j) {
  using instruments::Exercise;
  using instruments::OptionType;
  return {
      .option_type = j["type"] == "call" ? OptionType::kCall : OptionType::kPut,
      .exercise = j["exercise"] == "american" ? Exercise::kAmerican
                                              : Exercise::kEuropean,
      .strike = j["strike"],
      .maturity = j["maturity"]};
}

instruments::Portfolio MakeStrategy(const json& in) {
  namespace st = strategies;
  const std::string name = in["strategy"];
  const auto a = in["args"].get<std::vector<double>>();
  if (name == "bull_call_spread")
    return st::BullCallSpread(a[0], a[1], a[2], a[3]);
  if (name == "bear_put_spread")
    return st::BearPutSpread(a[0], a[1], a[2], a[3]);
  if (name == "butterfly_call_spread") {
    return st::ButterflyCallSpread(a[0], a[1], a[2], a[3], a[4], a[5]);
  }
  if (name == "straddle") return st::Straddle(a[0], a[1], a[2]);
  FAIL("unknown strategy: " << name);
  return {};
}

std::optional<pricers::TreeResult> EvaluateTree(const json& in) {
  const auto option = ParseOption(in["option"]);
  const int steps = in["steps"];
  const double rate = in["rate"];
  const double up = in["up"];
  const double down = in["down"];
  const double growth = std::exp(rate * option.maturity / steps);
  return pricers::PriceTree(
      option, in["spot"], rate, steps,
      {up, down, pricers::RiskNeutralProbability(up, down, growth)});
}

instruments::OptionType ParseType(const json& j) {
  return j == "call" ? instruments::OptionType::kCall
                     : instruments::OptionType::kPut;
}

std::vector<instruments::CashFlow> ParseFlows(const json& j) {
  std::vector<instruments::CashFlow> flows;
  for (const auto& f : j) flows.push_back({f[0], f[1]});
  return flows;
}

models::GeometricBrownianMotion ParseGbm(const json& in) {
  return {.drift = in["drift"], .volatility = in["volatility"]};
}

pricers::TreeMarket ParseMarket(const json& m) {
  return {.spot = m["spot"],
          .rate = m["rate"],
          .yield = m["yield"],
          .volatility = m["volatility"]};
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
  if (kind == "margin_cumulative_gain") {
    const auto prices = in["prices"].get<std::vector<double>>();
    return futures::SimulateMarginAccount(ParseMarginSpec(in), prices)
        .back()
        .cumulative_gain;
  }
  if (kind == "min_variance_hedge_ratio") {
    return risk::MinimumVarianceHedgeRatio(in["correlation"], in["spot_vol"],
                                           in["futures_vol"]);
  }
  if (kind == "optimal_contracts") {
    return risk::OptimalContracts(in["hedge_ratio"], in["exposure_units"],
                                  in["contract_units"]);
  }
  if (kind == "beta_hedge_contracts") {
    return risk::BetaHedgeContracts(in["beta"], in["portfolio_value"],
                                    in["futures_value"]);
  }
  if (kind == "change_beta_contracts") {
    return risk::ChangeBetaContracts(in["beta"], in["target_beta"],
                                     in["portfolio_value"],
                                     in["futures_value"]);
  }
  if (kind == "parse_thirty_seconds") {
    const auto q = futures::ParseThirtySeconds(in["quote"].get<std::string>());
    REQUIRE(q.has_value());
    return *q;
  }
  if (kind == "treasury_cash_price") {
    const auto q = futures::ParseThirtySeconds(in["quoted"].get<std::string>());
    REQUIRE(q.has_value());
    return futures::CashPrice(
        *q, futures::AccruedInterest(
                ParseDate(in["last_coupon"]), ParseDate(in["next_coupon"]),
                ParseDate(in["settlement"]), in["coupon"]));
  }
  if (kind == "conversion_factor") {
    return futures::ConversionFactor(in["coupon_rate"], in["years"]);
  }
  if (kind == "treasury_futures_quote") {
    const double r = in["rate"];
    const double cash =
        futures::CashPrice(in["quoted_bond"], in["accrued_now"]);
    const double coupons_pv = in["coupon"].get<double>() *
                              std::exp(-r * in["coupon_time"].get<double>());
    return futures::TreasuryFuturesQuote(cash, coupons_pv, r, in["t"],
                                         in["accrued_at_delivery"],
                                         in["conversion_factor"]);
  }
  if (kind == "convexity_adjustment") {
    return futures::ConvexityAdjustment(in["sigma"], in["t1"], in["t2"]);
  }
  if (kind == "duration_hedge_contracts") {
    return futures::DurationHedgeContracts(
        in["portfolio_value"], in["portfolio_duration"], in["futures_value"],
        in["futures_duration"]);
  }
  if (kind == "swap_fixed_leg") {
    return pricers::FixedLegBondValue(ParseSwap(in["swap"]),
                                      ParseCurve(in["curve"]));
  }
  if (kind == "swap_floating_leg") {
    return pricers::FloatingLegBondValue(ParseSwap(in["swap"]),
                                         ParseCurve(in["curve"]));
  }
  if (kind == "swap_value") {
    return pricers::SwapValue(ParseSwap(in["swap"]), ParseCurve(in["curve"]),
                              instruments::Side::kLong);
  }
  if (kind == "swap_value_as_fras") {
    return pricers::SwapValueAsFras(ParseSwap(in["swap"]),
                                    ParseCurve(in["curve"]),
                                    instruments::Side::kLong);
  }
  if (kind == "currency_swap_value") {
    const auto& j = in["swap"];
    const instruments::CurrencySwap swap{
        .domestic_notional = j["domestic_notional"],
        .domestic_rate = j["domestic_rate"],
        .foreign_notional = j["foreign_notional"],
        .foreign_rate = j["foreign_rate"],
        .frequency = j["frequency"],
        .payment_times = j["payment_times"].get<std::vector<double>>()};
    return pricers::CurrencySwapValue(
        swap, curves::ZeroCurve::Flat(in["domestic_rate"]),
        curves::ZeroCurve::Flat(in["foreign_rate"]), in["spot"],
        instruments::Side::kLong);
  }
  if (kind == "comparative_advantage") {
    return pricers::ComparativeAdvantageGain(in["fixed_a"], in["fixed_b"],
                                             in["floating_spread_a"],
                                             in["floating_spread_b"]);
  }
  if (kind == "naked_option_margin") {
    const auto type = in["option_type"] == "call"
                          ? instruments::OptionType::kCall
                          : instruments::OptionType::kPut;
    return in["units"].get<double>() *
           instruments::NakedOptionMargin(type, in["premium"], in["spot"],
                                          in["strike"]);
  }
  if (kind == "european_call_lower_bound") {
    return pricers::EuropeanCallLowerBound(in["spot"], in["strike"], in["rate"],
                                           in["t"]);
  }
  if (kind == "european_put_lower_bound") {
    return pricers::EuropeanPutLowerBound(in["spot"], in["strike"], in["rate"],
                                          in["t"]);
  }
  if (kind == "put_from_parity") {
    return pricers::PutFromParity(in["call"], in["spot"], in["strike"],
                                  in["rate"], in["t"]);
  }
  if (kind == "strategy_cost") return MakeStrategy(in).Cost();
  if (kind == "principal_protected_budget") {
    return strategies::PrincipalProtectedBudget(in["principal"], in["rate"],
                                                in["t"]);
  }
  if (kind == "tree_price" || kind == "tree_delta") {
    const auto result = EvaluateTree(in);
    REQUIRE(result.has_value());
    return kind == "tree_price" ? result->price : result->delta;
  }
  if (kind == "binomial_price") {
    const auto& m = in["market"];
    const pricers::TreeMarket market{.spot = m["spot"],
                                     .rate = m["rate"],
                                     .yield = m["yield"],
                                     .volatility = m["volatility"]};
    const std::string scheme = in["scheme"];
    const auto result = pricers::PriceBinomial(
        ParseOption(in["option"]), market, in["steps"],
        scheme == "crr"    ? pricers::TreeScheme::kCoxRossRubinstein
        : scheme == "tian" ? pricers::TreeScheme::kTian
                           : pricers::TreeScheme::kJarrowRudd);
    REQUIRE(result.has_value());
    return result->price;
  }
  if (kind == "gbm_expected_value") {
    return ParseGbm(in).ExpectedValue(in["s0"], in["t"]);
  }
  if (kind == "gbm_variance") return ParseGbm(in).Variance(in["s0"], in["t"]);
  if (kind == "historical_volatility") {
    const auto vol = stats::HistoricalVolatility(
        in["prices"].get<std::vector<double>>(), in["periods_per_year"]);
    REQUIRE(vol.has_value());
    return *vol;
  }
  if (kind == "bsm_price") {
    return pricers::BsmPrice(ParseType(in["type"]), in["spot"], in["strike"],
                             in["rate"], in["yield"], in["volatility"],
                             in["t"]);
  }
  if (kind == "implied_volatility") {
    const auto vol = pricers::ImpliedVolatility(
        ParseType(in["type"]), in["price"], in["spot"], in["strike"],
        in["rate"], in["yield"], in["t"]);
    REQUIRE(vol.has_value());
    return *vol;
  }
  if (kind == "bsm_price_with_dividends") {
    return pricers::BsmPriceWithDividends(
        ParseType(in["type"]), in["spot"], in["strike"], in["rate"],
        in["volatility"], in["t"], ParseFlows(in["dividends"]));
  }
  if (kind == "black_american_call") {
    return pricers::BlackAmericanCallApproximation(
        in["spot"], in["strike"], in["rate"], in["volatility"], in["t"],
        ParseFlows(in["dividends"]));
  }
  if (kind == "warrant_price") {
    return pricers::WarrantPrice(in["spot"], in["strike"], in["rate"],
                                 in["volatility"], in["t"], in["shares"],
                                 in["warrants"]);
  }
  if (kind == "garman_kohlhagen_price") {
    return pricers::GarmanKohlhagenPrice(
        ParseType(in["type"]), in["spot"], in["strike"], in["rate"],
        in["foreign_rate"], in["volatility"], in["t"]);
  }
  if (kind == "black76_price") {
    return pricers::Black76Price(ParseType(in["type"]), in["futures"],
                                 in["strike"], in["rate"], in["volatility"],
                                 in["t"]);
  }
  if (kind == "futures_put_from_parity") {
    return pricers::FuturesPutFromParity(in["call"], in["futures"],
                                         in["strike"], in["rate"], in["t"]);
  }
  if (kind == "control_variate_american") {
    const auto v = pricers::ControlVariateAmericanPrice(
        ParseOption(in["option"]), ParseMarket(in["market"]), in["steps"]);
    REQUIRE(v.has_value());
    return *v;
  }
  if (kind == "binomial_with_dividends") {
    const auto r = pricers::PriceBinomialWithDividends(
        ParseOption(in["option"]), ParseMarket(in["market"]), in["steps"],
        ParseFlows(in["dividends"]));
    REQUIRE(r.has_value());
    return r->price;
  }
  if (kind == "trinomial_price") {
    const auto r = pricers::PriceTrinomial(
        ParseOption(in["option"]), ParseMarket(in["market"]), in["steps"]);
    REQUIRE(r.has_value());
    return r->price;
  }
  if (kind == "finite_difference") {
    const auto& g = in["grid"];
    const pricers::FdGrid grid{.s_min = g["s_min"],
                               .s_max = g["s_max"],
                               .price_steps = g["price_steps"],
                               .time_steps = g["time_steps"],
                               .log_space = g["log_space"]};
    const std::string scheme = in["scheme"];
    const auto v = pricers::PriceFiniteDifference(
        ParseOption(in["option"]), ParseMarket(in["market"]), grid,
        scheme == "explicit"   ? pricers::FdScheme::kExplicit
        : scheme == "implicit" ? pricers::FdScheme::kImplicit
                               : pricers::FdScheme::kCrankNicolson,
        in["american"] == "projection" ? pricers::AmericanSolver::kProjection
                                       : pricers::AmericanSolver::kPsor);
    REQUIRE(v.has_value());
    return *v;
  }
  if (kind == "mc_european") {
    const std::string method = in["method"];
    const auto r = pricers::MonteCarloEuropean(
        ParseOption(in["option"]), ParseMarket(in["market"]),
        {.paths = in["paths"],
         .seed = in["seed"],
         .method = method == "antithetic"
                       ? pricers::VarianceReduction::kAntithetic
                       : pricers::VarianceReduction::kNone});
    REQUIRE(r.has_value());
    return r->price;
  }
  if (kind == "norm_inv") return math::NormInv(in["p"]);
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
  if (kind == "margin_balances" || kind == "margin_calls") {
    const auto prices = in["prices"].get<std::vector<double>>();
    std::vector<double> out;
    for (const auto& day :
         futures::SimulateMarginAccount(ParseMarginSpec(in), prices)) {
      out.push_back(kind == "margin_calls" ? day.margin_call : day.balance);
    }
    return out;
  }
  if (kind == "cheapest_to_deliver_costs") {
    const double f = in["futures"];
    std::vector<double> out;
    for (const auto& b : in["bonds"]) {
      const futures::DeliverableBond bond{b[0], b[1]};
      out.push_back(futures::FindCheapestToDeliver(f, {&bond, 1})->cost);
    }
    return out;
  }
  if (kind == "adjust_for_split") {
    const auto t = instruments::AdjustForSplit({in["strike"], in["shares"]},
                                               in["n"], in["m"]);
    return {t.strike, t.shares};
  }
  if (kind == "adjust_for_stock_dividend") {
    const auto t = instruments::AdjustForStockDividend(
        {in["strike"], in["shares"]}, in["fraction"]);
    return {t.strike, t.shares};
  }
  if (kind == "american_call_minus_put") {
    const auto r = pricers::AmericanCallMinusPut(in["spot"], in["strike"],
                                                 in["rate"], in["t"]);
    return {r.lower, r.upper};
  }
  if (kind == "strategy_profit") {
    const auto portfolio = MakeStrategy(in);
    std::vector<double> out;
    for (double s : in["spots"]) out.push_back(portfolio.ProfitAt(s));
    return out;
  }
  if (kind == "gbm_euler_path") {
    return ParseGbm(in).EulerPath(in["s0"], in["dt"],
                                  in["shocks"].get<std::vector<double>>());
  }
  if (kind == "gbm_confidence_interval") {
    const auto ci = ParseGbm(in).ConfidenceInterval(in["s0"], in["t"], in["z"]);
    return {ci.lower, ci.upper};
  }
  if (kind == "correlated_normals") {
    const auto lower =
        random::Cholesky(in["correlation"].get<random::Matrix>());
    REQUIRE(lower.has_value());
    return random::Correlate(*lower, in["z"].get<std::vector<double>>());
  }
  if (kind == "sobol_points") {
    auto sobol = random::Sobol::Create(in["dimensions"]);
    REQUIRE(sobol.has_value());
    std::vector<double> out;
    for (int i = 0; i < in["count"].get<int>(); ++i) {
      for (double x : sobol->Next()) out.push_back(x);
    }
    return out;
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

TEST_CASE("Golden: Chapter 2 futures markets", "[golden][ch02]") {
  RunChapter(2);
}

TEST_CASE("Golden: Chapter 3 hedging", "[golden][ch03]") { RunChapter(3); }

TEST_CASE("Golden: Chapter 6 interest rate futures", "[golden][ch06]") {
  RunChapter(6);
}

TEST_CASE("Golden: Chapter 7 swaps", "[golden][ch07]") { RunChapter(7); }

TEST_CASE("Golden: Chapter 10 options markets", "[golden][ch10]") {
  RunChapter(10);
}

TEST_CASE("Golden: Chapter 11 option properties", "[golden][ch11]") {
  RunChapter(11);
}

TEST_CASE("Golden: Chapter 12 trading strategies", "[golden][ch12]") {
  RunChapter(12);
}

TEST_CASE("Golden: Chapter 13 binomial trees", "[golden][ch13]") {
  RunChapter(13);
}

TEST_CASE("Golden: Chapter 14 Wiener processes", "[golden][ch14]") {
  RunChapter(14);
}

TEST_CASE("Golden: Chapter 15 Black-Scholes-Merton", "[golden][ch15]") {
  RunChapter(15);
}

TEST_CASE("Golden: Chapter 17 index and currency options", "[golden][ch17]") {
  RunChapter(17);
}

TEST_CASE("Golden: Chapter 18 futures options", "[golden][ch18]") {
  RunChapter(18);
}

TEST_CASE("Golden: Chapter 21 numerical procedures", "[golden][ch21]") {
  RunChapter(21);
}

TEST_CASE("Golden: Chapter 4 interest rates", "[golden][ch04]") {
  RunChapter(4);
}

TEST_CASE("Golden: Chapter 5 forward prices", "[golden][ch05]") {
  RunChapter(5);
}

}  // namespace
}  // namespace ofod
