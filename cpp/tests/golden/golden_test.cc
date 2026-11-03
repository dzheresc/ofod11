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
#include <utility>
#include <vector>

#include "ofod/core/compounding.h"
#include "ofod/core/date.h"
#include "ofod/credit/cds.h"
#include "ofod/credit/copula.h"
#include "ofod/credit/credit_risk.h"
#include "ofod/credit/xva.h"
#include "ofod/curves/bond_bootstrap.h"
#include "ofod/curves/credit_curve.h"
#include "ofod/curves/vol_surface.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/futures/margin.h"
#include "ofod/futures/rate_futures.h"
#include "ofod/futures/treasury.h"
#include "ofod/greeks/greeks.h"
#include "ofod/instruments/bond.h"
#include "ofod/instruments/cash_flow.h"
#include "ofod/instruments/option.h"
#include "ofod/instruments/portfolio.h"
#include "ofod/math/linalg.h"
#include "ofod/math/normal.h"
#include "ofod/models/brownian.h"
#include "ofod/models/lmm.h"
#include "ofod/models/short_rate.h"
#include "ofod/pricers/adjustments.h"
#include "ofod/pricers/american_mc.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/black_scholes.h"
#include "ofod/pricers/bond_pricer.h"
#include "ofod/pricers/bounds.h"
#include "ofod/pricers/eso.h"
#include "ofod/pricers/exotic_swaps.h"
#include "ofod/pricers/exotics.h"
#include "ofod/pricers/finite_difference.h"
#include "ofod/pricers/forward_pricer.h"
#include "ofod/pricers/mbs.h"
#include "ofod/pricers/model_pricers.h"
#include "ofod/pricers/monte_carlo.h"
#include "ofod/pricers/rate_options.h"
#include "ofod/pricers/short_rate_tree.h"
#include "ofod/pricers/smile.h"
#include "ofod/pricers/swap_pricer.h"
#include "ofod/random/normal_generator.h"
#include "ofod/random/sobol.h"
#include "ofod/risk/hedging.h"
#include "ofod/risk/var.h"
#include "ofod/stats/descriptive.h"
#include "ofod/stats/volatility.h"
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

greeks::OptionInputs ParseInputs(const json& in) {
  return {.spot = in["spot"],
          .rate = in["rate"],
          .yield = in["yield"],
          .volatility = in["volatility"],
          .maturity = in["t"]};
}

double PickGreek(const greeks::Greeks& g, const std::string& name) {
  if (name == "delta") return g.delta;
  if (name == "gamma") return g.gamma;
  if (name == "vega") return g.vega;
  if (name == "theta") return g.theta;
  return g.rho;
}

greeks::Exposure ParseExposure(const json& j) {
  return {.delta = j[0], .gamma = j[1], .vega = j[2]};
}

// Chapters 16, 19 and 20; std::nullopt when `kind` is not one of theirs.
std::optional<double> EvaluateM6(const std::string& kind, const json& in) {
  if (kind == "bsm_greek" || kind == "bump_greek") {
    const auto type = ParseType(in["type"]);
    const double strike = in["strike"];
    const auto inputs = ParseInputs(in);
    const auto g =
        kind == "bsm_greek"
            ? greeks::BsmGreeks(type, strike, inputs)
            : greeks::BumpGreeks(
                  [&](const greeks::OptionInputs& x) {
                    return pricers::BsmPrice(type, x.spot, strike, x.rate,
                                             x.yield, x.volatility, x.maturity);
                  },
                  inputs);
    return PickGreek(g, in["greek"]);
  }
  if (kind == "delta_hedge_ratio") {
    const auto r =
        greeks::SimulateDeltaHedge({.spot = in["spot"],
                                    .strike = in["strike"],
                                    .rate = in["rate"],
                                    .drift = in["drift"],
                                    .volatility = in["volatility"],
                                    .maturity = in["maturity"]},
                                   in["rebalances"], in["paths"], in["seed"]);
    REQUIRE(r.has_value());
    return r->ratio;
  }
  if (kind == "vol_surface") {
    const json& j = in["surface"];
    const auto surface = curves::VolSurface::Create(
        j["maturities"], j["strikes"], j["volatilities"],
        in["interpolation"].get<std::string>() == "linear_vol"
            ? curves::VolInterpolation::kLinearVol
            : curves::VolInterpolation::kTotalVariance);
    REQUIRE(surface.has_value());
    return surface->Volatility(in["strike"], in["t"]);
  }
  if (kind == "implied_density_bsm") {
    const double k = in["strike"];
    const double h = in["spacing"];
    auto call = [&](double strike) {
      return pricers::BsmPrice(instruments::OptionType::kCall, in["spot"],
                               strike, in["rate"], in["yield"],
                               in["volatility"], in["t"]);
    };
    return pricers::ImpliedDensity(call(k - h), call(k), call(k + h), h,
                                   in["rate"], in["t"]);
  }
  if (kind == "strike_from_delta") {
    const auto k = pricers::StrikeFromDelta(ParseType(in["type"]), in["delta"],
                                            in["spot"], in["rate"], in["yield"],
                                            in["volatility"], in["t"]);
    REQUIRE(k.has_value());
    return *k;
  }
  if (kind == "minimum_variance_delta") {
    return pricers::MinimumVarianceDelta(in["delta"], in["vega"], in["spot"],
                                         in["t"], in["a"], in["b"], in["c"]);
  }
  if (kind == "binary_jump_implied_vol") {
    const auto type = ParseType(in["type"]);
    const double price =
        pricers::BinaryJumpPrice(type, in["strike"], in["rate"], in["t"],
                                 in["up"], in["down"], in["p_up"]);
    const auto iv = pricers::ImpliedVolatility(
        type, price, in["spot"], in["strike"], in["rate"], 0.0, in["t"]);
    REQUIRE(iv.has_value());
    return *iv;
  }
  if (kind == "eso_tree") {
    const json& t = in["terms"];
    const auto v = pricers::PriceEmployeeStockOption(
        {.strike = t["strike"],
         .maturity = t["maturity"],
         .vesting = t["vesting"],
         .exercise_multiple = t["exercise_multiple"],
         .exit_rate = t["exit_rate"]},
        ParseMarket(in["market"]), in["steps"]);
    REQUIRE(v.has_value());
    return *v;
  }
  if (kind == "expected_life_value") {
    return pricers::ExpectedLifeValue(in["spot"], in["strike"], in["rate"],
                                      in["yield"], in["volatility"],
                                      in["expected_life"]);
  }
  return std::nullopt;
}

// Vector cases of chapters 16, 19 and 20.
std::optional<std::vector<double>> EvaluateM6Vector(const std::string& kind,
                                                    const json& in) {
  if (kind == "gamma_neutral_hedge" || kind == "gamma_vega_neutral_hedge") {
    const auto portfolio = ParseExposure(in["portfolio"]);
    const auto h =
        kind == "gamma_neutral_hedge"
            ? greeks::GammaNeutralHedge(portfolio, ParseExposure(in["option1"]))
            : greeks::GammaVegaNeutralHedge(portfolio,
                                            ParseExposure(in["option1"]),
                                            ParseExposure(in["option2"]));
    REQUIRE(h.has_value());
    return std::vector<double>{h->option1, h->option2, h->underlying};
  }
  return std::nullopt;
}

stats::Garch11 ParseGarch(const json& in) {
  return {.omega = in["omega"], .alpha = in["alpha"], .beta = in["beta"]};
}

// Chapters 22 and 23; std::nullopt when `kind` is not one of theirs.
std::optional<double> EvaluateM7(const std::string& kind, const json& in) {
  if (kind == "normal_var") {
    return risk::ScaleToHorizon(risk::NormalVar(in["sd"], in["confidence"]),
                                in["days"]);
  }
  if (kind == "normal_es") {
    return risk::NormalExpectedShortfall(in["sd"], in["confidence"]);
  }
  if (kind == "portfolio_std_dev" || kind == "portfolio_var") {
    const auto exposures = in["exposures"].get<std::vector<double>>();
    const auto vols = in["vols"].get<std::vector<double>>();
    const auto sd = risk::PortfolioStdDev(
        exposures, vols, in["correlation"].get<math::Matrix>());
    REQUIRE(sd.has_value());
    if (kind == "portfolio_std_dev") return *sd;
    return risk::ScaleToHorizon(risk::NormalVar(*sd, in["confidence"]),
                                in["days"]);
  }
  if (kind == "historical_var" || kind == "historical_es") {
    const auto losses = in["losses"].get<std::vector<double>>();
    const auto v =
        kind == "historical_var"
            ? risk::HistoricalVar(losses, in["confidence"])
            : risk::HistoricalExpectedShortfall(losses, in["confidence"]);
    REQUIRE(v.has_value());
    return *v;
  }
  if (kind == "cornish_fisher_var") {
    return risk::CornishFisherVar({.mean = in["mean"],
                                   .std_dev = in["std_dev"],
                                   .skewness = in["skewness"]},
                                  in["confidence"]);
  }
  if (kind == "delta_gamma_var") {
    return risk::CornishFisherVar(
        risk::DeltaGammaMoments(in["delta"], in["gamma"], in["sd_change"]),
        in["confidence"]);
  }
  if (kind == "kupiec") {
    return risk::KupiecStatistic(in["observations"], in["exceptions"], in["p"]);
  }
  if (kind == "binomial_tail") {
    return risk::BinomialTailProbability(in["n"], in["m"], in["p"]);
  }
  if (kind == "pca_std_dev") {
    const auto exposures = in["exposures"].get<std::vector<double>>();
    const auto sds = in["factor_sds"].get<std::vector<double>>();
    const auto sd =
        risk::PcaStdDev(exposures, in["loadings"].get<math::Matrix>(), sds);
    REQUIRE(sd.has_value());
    return *sd;
  }
  if (kind == "ewma_vol") {
    const double vol = in["vol"];
    return std::sqrt(
        stats::EwmaVarianceUpdate(vol * vol, in["ret"], in["lambda"]));
  }
  if (kind == "garch_long_run_variance") {
    return ParseGarch(in).LongRunVariance();
  }
  if (kind == "garch_update_vol") {
    const double vol = in["vol"];
    return std::sqrt(ParseGarch(in).Update(vol * vol, in["ret"]));
  }
  if (kind == "garch_expected_variance") {
    const double vol = in["vol"];
    return ParseGarch(in).ExpectedVariance(vol * vol, in["days"]);
  }
  if (kind == "garch_term_vol") {
    const double vol = in["vol"];
    return ParseGarch(in).TermStructureVolatility(vol * vol, in["days"]);
  }
  if (kind == "ewma_correlation") {
    const double vx = in["vol_x"];
    const double vy = in["vol_y"];
    const double lambda = in["lambda"];
    const double cov = stats::EwmaCovarianceUpdate(
        in["rho"].get<double>() * vx * vy, in["ret_x"], in["ret_y"], lambda);
    const double var_x =
        stats::EwmaVarianceUpdate(vx * vx, in["ret_x"], lambda);
    const double var_y =
        stats::EwmaVarianceUpdate(vy * vy, in["ret_y"], lambda);
    return cov / std::sqrt(var_x * var_y);
  }
  if (kind == "is_psd") {
    return stats::IsPositiveSemidefinite(in["matrix"].get<math::Matrix>())
               ? 1.0
               : 0.0;
  }
  if (kind == "min_eigenvalue") {
    const auto e = math::SymmetricEigen(in["matrix"].get<math::Matrix>());
    REQUIRE(e.has_value());
    return e->values.back();
  }
  if (kind == "garch_fit_log_likelihood") {
    const auto u = in["returns"].get<std::vector<double>>();
    const auto fit = stats::FitGarch11(u);
    REQUIRE(fit.has_value());
    return fit->LogLikelihood(u);
  }
  if (kind == "ewma_log_likelihood") {
    const auto u = in["returns"].get<std::vector<double>>();
    return stats::EwmaLogLikelihood(u, in["lambda"]);
  }
  if (kind == "ljung_box_squared" || kind == "ljung_box_garch") {
    const auto u = in["returns"].get<std::vector<double>>();
    std::vector<double> v(u.size(), 1.0);
    if (kind == "ljung_box_garch") {
      const auto fit = stats::FitGarch11(u);
      REQUIRE(fit.has_value());
      v = fit->Variances(u);
    }
    std::vector<double> z;
    for (std::size_t i = 1; i < u.size(); ++i) z.push_back(u[i] * u[i] / v[i]);
    const auto q = stats::LjungBox(z, in["lags"]);
    REQUIRE(q.has_value());
    return *q;
  }
  return std::nullopt;
}

// Vector cases of chapters 22 and 23.
std::optional<std::vector<double>> EvaluateM7Vector(const std::string& kind,
                                                    const json& in) {
  if (kind == "volatility_adjusted_changes") {
    const auto changes = in["changes"].get<std::vector<double>>();
    const auto vols = in["vols"].get<std::vector<double>>();
    auto out =
        risk::VolatilityAdjustedChanges(changes, vols, in["current_vol"]);
    REQUIRE(out.has_value());
    return out;
  }
  if (kind == "pca_variances" || kind == "pca_loading") {
    auto pca =
        risk::PrincipalComponents(in["observations"].get<math::Matrix>());
    REQUIRE(pca.has_value());
    if (kind == "pca_variances") return pca->variances;
    return pca->loadings[in["factor"].get<std::size_t>()];
  }
  if (kind == "garch_fit") {
    const auto u = in["returns"].get<std::vector<double>>();
    const auto fit = stats::FitGarch11(u);
    REQUIRE(fit.has_value());
    return std::vector<double>{fit->omega * 1e6, fit->alpha, fit->beta};
  }
  return std::nullopt;
}

credit::CdsTerms ParseCdsTerms(const json& in) {
  return {.maturity = in["maturity"],
          .frequency = in["frequency"],
          .recovery = in["recovery"]};
}

credit::CdoPool ParsePool(const json& j) {
  return {
      .names = j["names"], .hazard = j["hazard"], .recovery = j["recovery"]};
}

credit::CdoTerms ParseCdoTerms(const json& j) {
  return {.maturity = j["maturity"],
          .frequency = j["frequency"],
          .quadrature_points = j["quadrature_points"]};
}

credit::Tranche ParseTranche(const json& j) {
  return {.attachment = j[0], .detachment = j[1]};
}

std::optional<credit::ExposureProfile> ForwardExposure(const json& in) {
  return credit::SimulateForwardExposure(in["spot"], in["strike"], in["rate"],
                                         in["volatility"], in["t"], in["steps"],
                                         in["paths"], in["seed"], 0.975,
                                         in.value("threshold", 1e300));
}

// Chapters 9, 24 and 25; std::nullopt when `kind` is not one of theirs.
std::optional<double> EvaluateM8(const std::string& kind, const json& in) {
  if (kind == "hazard_from_spread") {
    return curves::HazardFromSpread(in["spread"], in["recovery"]);
  }
  if (kind == "average_hazard") {
    return curves::AverageHazard(in["cumulative_default"], in["t"]);
  }
  if (kind == "survival_probability") {
    const auto c = curves::CreditCurve::Create(in["times"], in["hazards"]);
    REQUIRE(c.has_value());
    return c->SurvivalProbability(in["t"]);
  }
  if (kind == "merton") {
    const auto m = credit::MertonSolve(in["equity"], in["equity_vol"],
                                       in["debt"], in["rate"], in["t"]);
    REQUIRE(m.has_value());
    const std::string out = in["output"];
    if (out == "asset_value") return m->asset_value;
    if (out == "asset_volatility") return m->asset_volatility;
    if (out == "default_probability") return m->default_probability;
    return m->debt_value;
  }
  if (kind == "bivariate_normal") {
    return math::BivariateNormCdf(in["a"], in["b"], in["rho"]);
  }
  if (kind == "joint_default") {
    return credit::JointDefaultProbability(in["q1"], in["q2"], in["rho"]);
  }
  if (kind == "wcdr")
    return credit::Wcdr(in["pd"], in["rho"], in["confidence"]);
  if (kind == "vasicek_credit_loss") {
    return credit::VasicekCreditLoss(in["exposure"], in["pd"], in["lgd"],
                                     in["rho"], in["confidence"]);
  }
  if (kind == "cds_leg" || kind == "cds_par_spread" ||
      kind == "cds_seller_value" || kind == "cds_upfront") {
    const auto terms = ParseCdsTerms(in);
    const auto credit_curve = curves::CreditCurve::Flat(in["hazard"]);
    const auto discount = curves::ZeroCurve::Flat(in["rate"]);
    std::optional<double> v;
    if (kind == "cds_leg") {
      const auto legs = credit::CdsLegValues(terms, credit_curve, discount);
      REQUIRE(legs.has_value());
      return in["output"] == "protection" ? legs->protection
                                          : legs->premium_annuity;
    }
    if (kind == "cds_par_spread") {
      v = credit::CdsParSpread(terms, credit_curve, discount);
    } else if (kind == "cds_seller_value") {
      const auto b =
          credit::CdsBuyerValue(terms, in["spread"], credit_curve, discount);
      if (b) v = -*b;
    } else {
      v = credit::CdsUpfront(terms, in["coupon"], credit_curve, discount);
    }
    REQUIRE(v.has_value());
    return *v;
  }
  if (kind == "cds_option") {
    return credit::CdsOptionPrice(in["payer"], in["forward"], in["strike"],
                                  in["vol"], in["expiry"], in["annuity"]);
  }
  if (kind == "tranche_spread_bp" || kind == "implied_correlation") {
    const auto pool = ParsePool(in["pool"]);
    const auto terms = ParseCdoTerms(in["terms"]);
    const auto tranche = ParseTranche(in["tranche"]);
    const auto discount = curves::ZeroCurve::Flat(in["rate"]);
    const auto spread = credit::TrancheSpread(tranche, pool, in["correlation"],
                                              terms, discount);
    REQUIRE(spread.has_value());
    if (kind == "tranche_spread_bp") return 1e4 * *spread;
    const auto rho = credit::ImpliedTrancheCorrelation(tranche, pool, *spread,
                                                       terms, discount);
    REQUIRE(rho.has_value());
    return *rho;
  }
  if (kind == "nth_to_default_bp") {
    const auto r =
        credit::NthToDefaultSpread({.names = in["names"],
                                    .nth = in["nth"],
                                    .hazard = in["hazard"],
                                    .recovery = in["recovery"],
                                    .correlation = in["correlation"],
                                    .maturity = in["maturity"],
                                    .frequency = in["frequency"]},
                                   in["rate"], in["paths"], in["seed"]);
    REQUIRE(r.has_value());
    return 1e4 * r->spread;
  }
  if (kind == "cva") {
    const auto times = in["times"].get<std::vector<double>>();
    const auto exposures = in["exposures"].get<std::vector<double>>();
    const auto v =
        credit::Cva(times, exposures, curves::CreditCurve::Flat(in["hazard"]),
                    in["recovery"]);
    REQUIRE(v.has_value());
    return *v;
  }
  if (kind == "fva") {
    const auto times = in["times"].get<std::vector<double>>();
    const auto exposures = in["exposures"].get<std::vector<double>>();
    const auto v =
        credit::FundingValueAdjustment(times, exposures, in["spread"]);
    REQUIRE(v.has_value());
    return *v;
  }
  if (kind == "forward_cva") {
    // Exposure at T is a call on the forward; default only at T.
    const double ee = pricers::BsmPrice(instruments::OptionType::kCall,
                                        in["spot"], in["strike"], in["rate"],
                                        0.0, in["volatility"], in["t"]);
    const std::vector<double> times = {in["t"].get<double>()};
    const std::vector<double> exposures = {ee};
    const auto v =
        credit::Cva(times, exposures, curves::CreditCurve::Flat(in["hazard"]),
                    in["recovery"]);
    REQUIRE(v.has_value());
    return *v;
  }
  if (kind == "forward_ee_mc") {
    const auto p = ForwardExposure(in);
    REQUIRE(p.has_value());
    return p->expected_exposure.back();
  }
  return std::nullopt;
}

// Vector cases of chapters 9, 24 and 25.
std::optional<std::vector<double>> EvaluateM8Vector(const std::string& kind,
                                                    const json& in) {
  if (kind == "cds_bootstrap_reprice" || kind == "cds_bootstrap_hazards") {
    const auto mats = in["maturities"].get<std::vector<double>>();
    const auto spreads = in["spreads"].get<std::vector<double>>();
    const auto discount = curves::ZeroCurve::Flat(in["rate"]);
    const auto curve = credit::BootstrapCreditCurve(
        mats, spreads, in["frequency"], in["recovery"], discount);
    REQUIRE(curve.has_value());
    if (kind == "cds_bootstrap_hazards") {
      return std::vector<double>(curve->hazards().begin(),
                                 curve->hazards().end());
    }
    std::vector<double> out;
    for (double m : mats) {
      const auto s = credit::CdsParSpread({.maturity = m,
                                           .frequency = in["frequency"],
                                           .recovery = in["recovery"]},
                                          *curve, discount);
      REQUIRE(s.has_value());
      out.push_back(*s);
    }
    return out;
  }
  return std::nullopt;
}

pricers::TreeMarket ParseTreeMarket(const json& j) {
  return {.spot = j["spot"],
          .rate = j["rate"],
          .yield = j["yield"],
          .volatility = j["volatility"]};
}

double Value(std::optional<double> v) {
  REQUIRE(v.has_value());
  return *v;
}

pricers::CompoundType ParseCompound(const std::string& s) {
  if (s == "call_on_call") return pricers::CompoundType::kCallOnCall;
  if (s == "put_on_call") return pricers::CompoundType::kPutOnCall;
  if (s == "call_on_put") return pricers::CompoundType::kCallOnPut;
  return pricers::CompoundType::kPutOnPut;
}

pricers::BarrierType ParseBarrier(const std::string& s) {
  if (s == "down_and_out") return pricers::BarrierType::kDownAndOut;
  if (s == "down_and_in") return pricers::BarrierType::kDownAndIn;
  if (s == "up_and_out") return pricers::BarrierType::kUpAndOut;
  return pricers::BarrierType::kUpAndIn;
}

// Chapters 26 and 27; std::nullopt when `kind` is not one of theirs.
std::optional<double> EvaluateM9(const std::string& kind, const json& in) {
  namespace p = pricers;
  if (kind == "exchange") {
    return p::ExchangeOptionPrice({.receive_spot = in["receive_spot"],
                                   .deliver_spot = in["deliver_spot"],
                                   .receive_yield = in["receive_yield"],
                                   .deliver_yield = in["deliver_yield"],
                                   .receive_vol = in["receive_vol"],
                                   .deliver_vol = in["deliver_vol"],
                                   .correlation = in["correlation"]},
                                  in["t"]);
  }
  if (!in.contains("market")) return std::nullopt;
  const auto m = ParseTreeMarket(in["market"]);
  const auto type =
      in.contains("type") ? ParseType(in["type"]) : p::OptionType::kCall;
  if (kind == "gap_option") {
    return p::GapOptionPrice(type, m, in["strike"], in["trigger"], in["t"]);
  }
  if (kind == "floating_lookback") {
    return Value(p::FloatingLookbackPrice(type, m, in["extreme"], in["t"]));
  }
  if (kind == "fixed_lookback") {
    return Value(
        p::FixedLookbackPrice(type, m, in["strike"], in["extreme"], in["t"]));
  }
  if (kind == "arithmetic_asian") {
    return Value(p::ArithmeticAsianPrice(type, m, in["strike"], in["t"]));
  }
  if (kind == "geometric_asian") {
    return p::GeometricAsianPrice(type, m, in["strike"], in["t"]);
  }
  if (kind == "chooser") {
    return p::ChooserPrice(m, in["strike"], in["t1"], in["t2"]);
  }
  if (kind == "forward_start") {
    return p::ForwardStartPrice(type, m, in["t1"], in["t2"]);
  }
  if (kind == "perpetual_american") {
    return Value(p::PerpetualAmericanPrice(type, m, in["strike"]));
  }
  if (kind == "cash_or_nothing") {
    return p::CashOrNothingPrice(type, m, in["strike"], in["t"], in["cash"]);
  }
  if (kind == "asset_or_nothing") {
    return p::AssetOrNothingPrice(type, m, in["strike"], in["t"]);
  }
  if (kind == "compound") {
    return Value(p::CompoundOptionPrice(ParseCompound(in["compound"]), m,
                                        in["k1"], in["t1"], in["k2"],
                                        in["t2"]));
  }
  if (kind == "barrier") {
    return p::BarrierOptionPrice(ParseBarrier(in["barrier"]), type, m,
                                 in["strike"], in["level"], in["t"]);
  }
  if (kind == "variance_swap") {
    const double lo = in["k_lo"];
    const double step = in["k_step"];
    const auto n =
        static_cast<int>(std::lround((in["k_hi"].get<double>() - lo) / step));
    std::vector<double> strikes;
    for (int k = 0; k <= n; ++k) strikes.push_back(lo + k * step);
    const double atm = in["atm_vol"];
    const double skew = in["skew"];
    return Value(p::VarianceSwapFairVariance(
        m, in["t"], strikes,
        [atm, skew](double k) { return atm + skew * std::log(k / 100.0); }));
  }
  if (kind == "convertible") {
    return Value(
        p::ConvertibleBondPrice(m,
                                {.face = in["face"],
                                 .conversion_ratio = in["conversion_ratio"],
                                 .maturity = in["maturity"],
                                 .call_price = in["call_price"],
                                 .recovery = in["recovery"],
                                 .hazard = in["hazard"]},
                                in["steps"]));
  }
  if (kind == "lsm") {
    const auto r = p::LongstaffSchwartz(type, m, in["strike"], in["t"],
                                        {.exercise_dates = in["exercise_dates"],
                                         .paths = in["paths"],
                                         .seed = in["seed"],
                                         .basis_degree = in["basis_degree"]});
    REQUIRE(r.has_value());
    return r->price;
  }
  if (kind == "cev") {
    return Value(p::CevPrice(type, m, in["strike"], in["t"], in["alpha"]));
  }
  if (kind == "merton_jump") {
    return p::MertonJumpPrice(type, m, in["strike"], in["t"],
                              {.intensity = in["intensity"],
                               .mean_jump = in["mean_jump"],
                               .jump_vol = in["jump_vol"]});
  }
  if (kind == "heston" || kind == "heston_mc") {
    const p::HestonParams hp{.v0 = in["v0"],
                             .kappa = in["kappa"],
                             .theta = in["theta"],
                             .sigma = in["sigma"],
                             .rho = in["rho"]};
    if (kind == "heston") {
      return p::HestonPrice(type, m, in["strike"], in["t"], hp);
    }
    const auto r = p::HestonMonteCarlo(type, m, in["strike"], in["t"], hp,
                                       in["steps"], in["paths"], in["seed"]);
    REQUIRE(r.has_value());
    return r->price;
  }
  return std::nullopt;
}

std::vector<pricers::SwapPeriod> ParsePeriods(const json& in) {
  const json& s = in["schedule"];
  return pricers::SwapSchedule(
      s["start"], s["maturity"], s["frequency"], s["notional"], s["fixed_rate"],
      s.value("amortization", 0.0), s.value("step_up", 0.0));
}

std::optional<pricers::ShortRateTree> ParseTree(const json& in) {
  const auto model =
      in.value("model", std::string("hull_white")) == "black_karasinski"
          ? pricers::ShortRateModel::kBlackKarasinski
          : pricers::ShortRateModel::kHullWhite;
  return pricers::ShortRateTree::Build(ParseCurve(in["curve"]), in["a"],
                                       in["sigma"], in["dt"], in["steps"],
                                       model);
}

std::optional<double> EvaluateM10(const std::string& kind, const json& in) {
  namespace p = pricers;
  namespace md = models;
  const auto type =
      in.contains("type") ? ParseType(in["type"]) : p::OptionType::kCall;
  if (kind == "black_bond_option") {
    const double f =
        p::ForwardBondPrice(in["spot_price"], in["coupon_pv"], in["discount"]);
    return p::BlackBondOptionPrice(type, f, in["strike"], in["discount"],
                                   in["vol"], in["t"]);
  }
  if (kind == "black_caplet") {
    return p::BlackCapletPrice(type, in["forward"], in["strike"], in["vol"],
                               in["reset"], in["accrual"], in["discount"],
                               in["notional"]);
  }
  if (kind == "cap_floor") {
    return Value(p::CapFloorPrice(type, ParseCurve(in["curve"]),
                                  {.strike = in["strike"],
                                   .maturity = in["maturity"],
                                   .frequency = in["frequency"]},
                                  in["vol"]));
  }
  if (kind == "black_swaption" || kind == "bachelier_swaption") {
    const auto swap = p::ForwardSwapRate(ParseCurve(in["curve"]), in["start"],
                                         in["tenor"], in["frequency"]);
    const auto price = kind == "black_swaption" ? p::BlackSwaptionPrice
                                                : p::BachelierSwaptionPrice;
    return price(type, swap.rate, in["strike"], in["vol"], in["start"],
                 swap.annuity, in["notional"]);
  }
  if (kind == "yield_convexity_adjustment") {
    return p::BondYieldConvexityAdjustment(in["forward_yield"], in["vol"],
                                           in["t"], in["coupon_rate"],
                                           in["maturity"], in["frequency"]);
  }
  if (kind == "timing_factor") {
    return p::TimingAdjustmentFactor(in["forward_rate"], in["forward_vol"],
                                     in["variable_vol"], in["correlation"],
                                     in["t"], in["tau"]);
  }
  if (kind == "quanto_factor") {
    return p::QuantoAdjustmentFactor(in["correlation"], in["exchange_vol"],
                                     in["variable_vol"], in["t"]);
  }
  if (kind == "in_arrears_rate") {
    return p::InArrearsRate(in["forward_rate"], in["vol"], in["t"], in["tau"]);
  }
  if (kind == "cms_rate") {
    const json& t = in["terms"];
    return p::CmsRate({.forward_swap_rate = t["forward_swap_rate"],
                       .swap_volatility = t["swap_volatility"],
                       .t = t["t"],
                       .swap_tenor = t["swap_tenor"],
                       .frequency = t["frequency"],
                       .delay = t["delay"],
                       .delay_rate = t["delay_rate"],
                       .delay_volatility = t["delay_volatility"],
                       .correlation = t["correlation"]});
  }
  if (kind == "vasicek_bond") {
    return md::BondPrice(md::Vasicek{in["a"], in["b"], in["sigma"]}, in["r"],
                         in["tau"]);
  }
  if (kind == "cir_bond") {
    return md::BondPrice(md::Cir{in["a"], in["b"], in["sigma"]}, in["r"],
                         in["tau"]);
  }
  if (kind == "vasicek_bond_option") {
    return md::ZeroBondOptionPrice(md::Vasicek{in["a"], in["b"], in["sigma"]},
                                   type, in["r"], in["expiry"], in["maturity"],
                                   in["strike"]);
  }
  if (kind.starts_with("hw_")) {
    const auto hw =
        md::HullWhite::Create(ParseCurve(in["curve"]), in["a"], in["sigma"]);
    REQUIRE(hw.has_value());
    if (kind == "hw_zero_bond_option") {
      return hw->ZeroBondOption(type, in["expiry"], in["maturity"],
                                in["strike"]);
    }
    if (kind == "hw_coupon_bond_option") {
      return Value(hw->CouponBondOption(
          type, in["expiry"], in["times"].get<std::vector<double>>(),
          in["amounts"].get<std::vector<double>>(), in["strike"]));
    }
    if (kind == "hw_caplet") {
      return hw->Caplet(type, in["reset"], in["accrual"], in["strike"],
                        in["notional"]);
    }
    return std::nullopt;
  }
  if (kind == "tree_swaption" || kind == "tree_bond_option") {
    const auto tree = ParseTree(in);
    REQUIRE(tree.has_value());
    if (kind == "tree_swaption") {
      return Value(p::TreeSwaption(
          *tree, type, in["strike"], in["period_steps"], in["end_step"],
          in["exercise_steps"].get<std::vector<int>>()));
    }
    std::vector<p::TreeCashFlow> bond;
    for (const auto& c : in["bond"]) bond.push_back({c[0], c[1]});
    const auto exercise = in["exercise"] == "american"
                              ? instruments::Exercise::kAmerican
                              : instruments::Exercise::kEuropean;
    return Value(p::TreeBondOption(*tree, type, exercise, in["expiry_step"],
                                   bond, in["strike"]));
  }
  if (kind == "rebonato_vol") {
    const md::LmmSpec spec{
        .accrual = in["accrual"],
        .forwards = in["forwards"].get<std::vector<double>>(),
        .loadings = in["loadings"].get<md::Matrix>()};
    return Value(md::RebonatoSwaptionVolatility(spec, in["start"], in["end"]));
  }
  if (kind == "schedule_swap") {
    return p::ScheduleSwapValue(ParseCurve(in["curve"]), ParsePeriods(in));
  }
  if (kind == "in_arrears_swap" || kind == "cms_swap") {
    const auto periods = ParsePeriods(in);
    const std::vector<double> vols(periods.size(), in["vol"].get<double>());
    if (kind == "in_arrears_swap") {
      return p::InArrearsSwapValue(ParseCurve(in["curve"]), periods, vols);
    }
    return p::CmsSwapValue(ParseCurve(in["curve"]), periods,
                           {.cms_tenor = in["cms_tenor"],
                            .cms_frequency = in["cms_frequency"],
                            .forward_vol = in["forward_vol"],
                            .correlation = in["correlation"]},
                           vols);
  }
  if (kind == "accrual_fixed_leg") {
    return p::AccrualFixedLegValue(ParseCurve(in["curve"]), ParsePeriods(in),
                                   in["low"], in["high"], in["vol"],
                                   in["observations"]);
  }
  return std::nullopt;
}

std::optional<std::vector<double>> EvaluateM10Vector(const std::string& kind,
                                                     const json& in) {
  if (kind == "strip_caplet_vols") {
    auto v = pricers::StripCapletVolatilities(
        ParseCurve(in["curve"]), in["strike"], in["frequency"],
        in["flat_vols"].get<std::vector<double>>());
    REQUIRE(v.has_value());
    return v;
  }
  if (kind == "tree_alphas") {
    const auto tree = ParseTree(in);
    REQUIRE(tree.has_value());
    return std::vector<double>(tree->alphas().begin(), tree->alphas().end());
  }
  if (kind == "lmm_lambdas") {
    auto v = models::LmmVolatilitiesFromCaplets(
        in["caplet_vols"].get<std::vector<double>>());
    REQUIRE(v.has_value());
    return v;
  }
  if (kind == "mbs_psa") {
    const pricers::MortgagePool pool{
        .balance = in["balance"],
        .mortgage_rate = in["mortgage_rate"],
        .pass_through_rate = in["pass_through_rate"],
        .months = in["months"]};
    const auto v =
        pricers::ValueCashFlows(pricers::PassThroughCashFlows(pool, in["psa"]),
                                ParseCurve(in["curve"]));
    return std::vector<double>{v.pass_through, v.interest_only,
                               v.principal_only};
  }
  return std::nullopt;
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
  if (const auto v = EvaluateM6(kind, in)) return *v;
  if (const auto v = EvaluateM7(kind, in)) return *v;
  if (const auto v = EvaluateM8(kind, in)) return *v;
  if (const auto v = EvaluateM9(kind, in)) return *v;
  if (const auto v = EvaluateM10(kind, in)) return *v;
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
  if (auto v = EvaluateM6Vector(kind, in)) return *std::move(v);
  if (auto v = EvaluateM7Vector(kind, in)) return *std::move(v);
  if (auto v = EvaluateM8Vector(kind, in)) return *std::move(v);
  if (auto v = EvaluateM10Vector(kind, in)) return *std::move(v);
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

TEST_CASE("Golden: Chapter 16 employee stock options", "[golden][ch16]") {
  RunChapter(16);
}

TEST_CASE("Golden: Chapter 19 Greek letters", "[golden][ch19]") {
  RunChapter(19);
}

TEST_CASE("Golden: Chapter 20 volatility smiles", "[golden][ch20]") {
  RunChapter(20);
}

TEST_CASE("Golden: Chapter 21 numerical procedures", "[golden][ch21]") {
  RunChapter(21);
}

TEST_CASE("Golden: Chapter 22 value at risk", "[golden][ch22]") {
  RunChapter(22);
}

TEST_CASE("Golden: Chapter 23 volatilities and correlations",
          "[golden][ch23]") {
  RunChapter(23);
}

TEST_CASE("Golden: Chapter 9 XVAs", "[golden][ch09]") { RunChapter(9); }

TEST_CASE("Golden: Chapter 24 credit risk", "[golden][ch24]") {
  RunChapter(24);
}

TEST_CASE("Golden: Chapter 25 credit derivatives", "[golden][ch25]") {
  RunChapter(25);
}

TEST_CASE("Golden: Chapter 4 interest rates", "[golden][ch04]") {
  RunChapter(4);
}

TEST_CASE("Golden: Chapter 5 forward prices", "[golden][ch05]") {
  RunChapter(5);
}

TEST_CASE("Golden: Chapter 26 exotic options", "[golden][ch26]") {
  RunChapter(26);
}

TEST_CASE("Golden: Chapter 27 more on models", "[golden][ch27]") {
  RunChapter(27);
}

TEST_CASE("Golden: Chapter 29 standard market models", "[golden][ch29]") {
  RunChapter(29);
}

TEST_CASE("Golden: Chapter 30 convexity, timing and quanto adjustments",
          "[golden][ch30]") {
  RunChapter(30);
}

TEST_CASE("Golden: Chapter 31 equilibrium short-rate models",
          "[golden][ch31]") {
  RunChapter(31);
}

TEST_CASE("Golden: Chapter 32 no-arbitrage short-rate models",
          "[golden][ch32]") {
  RunChapter(32);
}

TEST_CASE("Golden: Chapter 33 modeling forward rates", "[golden][ch33]") {
  RunChapter(33);
}

TEST_CASE("Golden: Chapter 34 swaps revisited", "[golden][ch34]") {
  RunChapter(34);
}

}  // namespace
}  // namespace ofod
