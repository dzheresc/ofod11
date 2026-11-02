// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "bindings.h"
#include "ofod/credit/cds.h"
#include "ofod/credit/copula.h"
#include "ofod/credit/credit_risk.h"
#include "ofod/credit/tranche.h"
#include "ofod/credit/xva.h"
#include "ofod/curves/credit_curve.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/math/linalg.h"

namespace ofod::bindings {

namespace nb = nanobind;

namespace {

constexpr const char* kBadTerms = "need maturity > 0 and frequency >= 1";
constexpr const char* kInvalid = "invalid pool, tranche, correlation or terms";
constexpr const char* kSizes = "times and exposures differ in size";

// Returns *v, or raises ValueError(message).
template <typename T>
T Unwrap(std::optional<T> v, const char* message) {
  if (!v) throw nb::value_error(message);
  return *std::move(v);
}

void BindDefaultModels(nb::module_& m) {
  using credit::MertonResult;
  nb::class_<MertonResult>(m, "MertonResult")
      .def(nb::init<double, double, double, double>(), nb::arg("asset_value"),
           nb::arg("asset_volatility"), nb::arg("default_probability"),
           nb::arg("debt_value"))
      .def_rw("asset_value", &MertonResult::asset_value)
      .def_rw("asset_volatility", &MertonResult::asset_volatility)
      .def_rw("default_probability", &MertonResult::default_probability)
      .def_rw("debt_value", &MertonResult::debt_value);
  m.def(
      "merton_solve",
      [](double equity, double equity_vol, double debt, double rate, double t) {
        return Unwrap(credit::MertonSolve(equity, equity_vol, debt, rate, t),
                      "Merton solve failed: check that inputs are positive");
      },
      nb::arg("equity"), nb::arg("equity_vol"), nb::arg("debt"),
      nb::arg("rate"), nb::arg("t"));
  m.def("joint_default_probability", &credit::JointDefaultProbability,
        nb::arg("q1"), nb::arg("q2"), nb::arg("rho"));
  m.def("conditional_default_probability",
        &credit::ConditionalDefaultProbability, nb::arg("q"), nb::arg("rho"),
        nb::arg("factor"));
  m.def("wcdr", &credit::Wcdr, nb::arg("pd"), nb::arg("rho"),
        nb::arg("confidence"));
  m.def("vasicek_credit_loss", &credit::VasicekCreditLoss, nb::arg("exposure"),
        nb::arg("pd"), nb::arg("lgd"), nb::arg("rho"), nb::arg("confidence"));
}

void BindCds(nb::module_& m) {
  using credit::CdsLegs;
  using credit::CdsTerms;
  using curves::CreditCurve;
  using curves::ZeroCurve;

  nb::class_<CdsTerms>(m, "CdsTerms")
      .def(
          "__init__",
          [](CdsTerms* self, double maturity, int frequency, double recovery) {
            new (self) CdsTerms{maturity, frequency, recovery};
          },
          nb::arg("maturity") = 5.0, nb::arg("frequency") = 4,
          nb::arg("recovery") = 0.4)
      .def_rw("maturity", &CdsTerms::maturity)
      .def_rw("frequency", &CdsTerms::frequency)
      .def_rw("recovery", &CdsTerms::recovery);
  nb::class_<CdsLegs>(m, "CdsLegs")
      .def(nb::init<double, double>(), nb::arg("premium_annuity"),
           nb::arg("protection"))
      .def_rw("premium_annuity", &CdsLegs::premium_annuity)
      .def_rw("protection", &CdsLegs::protection);
  m.def(
      "cds_leg_values",
      [](const CdsTerms& terms, const CreditCurve& credit,
         const ZeroCurve& discount) {
        return Unwrap(credit::CdsLegValues(terms, credit, discount), kBadTerms);
      },
      nb::arg("terms"), nb::arg("credit"), nb::arg("discount"));
  m.def(
      "cds_par_spread",
      [](const CdsTerms& terms, const CreditCurve& credit,
         const ZeroCurve& discount) {
        return Unwrap(credit::CdsParSpread(terms, credit, discount), kBadTerms);
      },
      nb::arg("terms"), nb::arg("credit"), nb::arg("discount"));
  m.def(
      "cds_buyer_value",
      [](const CdsTerms& terms, double spread, const CreditCurve& credit,
         const ZeroCurve& discount) {
        return Unwrap(credit::CdsBuyerValue(terms, spread, credit, discount),
                      kBadTerms);
      },
      nb::arg("terms"), nb::arg("spread"), nb::arg("credit"),
      nb::arg("discount"));
  m.def(
      "cds_upfront",
      [](const CdsTerms& terms, double coupon, const CreditCurve& credit,
         const ZeroCurve& discount) {
        return Unwrap(credit::CdsUpfront(terms, coupon, credit, discount),
                      kBadTerms);
      },
      nb::arg("terms"), nb::arg("coupon"), nb::arg("credit"),
      nb::arg("discount"));
  m.def(
      "bootstrap_credit_curve",
      [](const std::vector<double>& maturities,
         const std::vector<double>& spreads, int frequency, double recovery,
         const ZeroCurve& discount) {
        return Unwrap(credit::BootstrapCreditCurve(
                          maturities, spreads, frequency, recovery, discount),
                      "invalid quotes or no hazard reprices a spread");
      },
      nb::arg("maturities"), nb::arg("spreads"), nb::arg("frequency"),
      nb::arg("recovery"), nb::arg("discount"));
  m.def("cds_option_price", &credit::CdsOptionPrice, nb::arg("payer"),
        nb::arg("forward_spread"), nb::arg("strike"), nb::arg("volatility"),
        nb::arg("expiry"), nb::arg("forward_annuity"));
}

void BindCopula(nb::module_& m) {
  using credit::BasketResult;
  using credit::BasketTerms;
  using credit::CdoPool;
  using credit::CdoTerms;
  using credit::Tranche;
  using credit::TrancheLegs;
  using curves::ZeroCurve;

  m.def(
      "default_count_distribution",
      [](const std::vector<double>& p) {
        return credit::DefaultCountDistribution(p);
      },
      nb::arg("p"));
  nb::class_<CdoPool>(m, "CdoPool")
      .def(
          "__init__",
          [](CdoPool* self, int names, double hazard, double recovery) {
            new (self) CdoPool{names, hazard, recovery};
          },
          nb::arg("names") = 125, nb::arg("hazard") = 0.01,
          nb::arg("recovery") = 0.4)
      .def_rw("names", &CdoPool::names)
      .def_rw("hazard", &CdoPool::hazard)
      .def_rw("recovery", &CdoPool::recovery);
  nb::class_<CdoTerms>(m, "CdoTerms")
      .def(
          "__init__",
          [](CdoTerms* self, double maturity, int frequency, int points) {
            new (self) CdoTerms{maturity, frequency, points};
          },
          nb::arg("maturity") = 5.0, nb::arg("frequency") = 4,
          nb::arg("quadrature_points") = 64)
      .def_rw("maturity", &CdoTerms::maturity)
      .def_rw("frequency", &CdoTerms::frequency)
      .def_rw("quadrature_points", &CdoTerms::quadrature_points);
  nb::class_<TrancheLegs>(m, "TrancheLegs")
      .def(nb::init<double, double>(), nb::arg("premium_annuity"),
           nb::arg("protection"))
      .def_rw("premium_annuity", &TrancheLegs::premium_annuity)
      .def_rw("protection", &TrancheLegs::protection);
  m.def(
      "expected_tranche_loss",
      [](const Tranche& tranche, const CdoPool& pool, double correlation,
         double t, int quadrature_points) {
        return Unwrap(credit::ExpectedTrancheLoss(tranche, pool, correlation, t,
                                                  quadrature_points),
                      kInvalid);
      },
      nb::arg("tranche"), nb::arg("pool"), nb::arg("correlation"), nb::arg("t"),
      nb::arg("quadrature_points") = 64);
  m.def(
      "tranche_leg_values",
      [](const Tranche& tranche, const CdoPool& pool, double correlation,
         const CdoTerms& terms, const ZeroCurve& discount) {
        return Unwrap(credit::TrancheLegValues(tranche, pool, correlation,
                                               terms, discount),
                      kInvalid);
      },
      nb::arg("tranche"), nb::arg("pool"), nb::arg("correlation"),
      nb::arg("terms"), nb::arg("discount"));
  m.def(
      "tranche_spread",
      [](const Tranche& tranche, const CdoPool& pool, double correlation,
         const CdoTerms& terms, const ZeroCurve& discount) {
        return Unwrap(
            credit::TrancheSpread(tranche, pool, correlation, terms, discount),
            kInvalid);
      },
      nb::arg("tranche"), nb::arg("pool"), nb::arg("correlation"),
      nb::arg("terms"), nb::arg("discount"));
  m.def(
      "implied_tranche_correlation",
      [](const Tranche& tranche, const CdoPool& pool, double spread,
         const CdoTerms& terms, const ZeroCurve& discount) {
        return Unwrap(credit::ImpliedTrancheCorrelation(tranche, pool, spread,
                                                        terms, discount),
                      "no implied correlation found");
      },
      nb::arg("tranche"), nb::arg("pool"), nb::arg("spread"), nb::arg("terms"),
      nb::arg("discount"));
  nb::class_<BasketTerms>(m, "BasketTerms")
      .def(
          "__init__",
          [](BasketTerms* self, int names, int nth, double hazard,
             double recovery, double correlation, double maturity,
             int frequency) {
            new (self) BasketTerms{names,       nth,      hazard,   recovery,
                                   correlation, maturity, frequency};
          },
          nb::arg("names") = 10, nb::arg("nth") = 1, nb::arg("hazard") = 0.01,
          nb::arg("recovery") = 0.4, nb::arg("correlation") = 0.3,
          nb::arg("maturity") = 5.0, nb::arg("frequency") = 4)
      .def_rw("names", &BasketTerms::names)
      .def_rw("nth", &BasketTerms::nth)
      .def_rw("hazard", &BasketTerms::hazard)
      .def_rw("recovery", &BasketTerms::recovery)
      .def_rw("correlation", &BasketTerms::correlation)
      .def_rw("maturity", &BasketTerms::maturity)
      .def_rw("frequency", &BasketTerms::frequency);
  nb::class_<BasketResult>(m, "BasketResult")
      .def(nb::init<double, double>(), nb::arg("spread"), nb::arg("std_error"))
      .def_rw("spread", &BasketResult::spread)
      .def_rw("std_error", &BasketResult::std_error);
  m.def(
      "nth_to_default_spread",
      [](const BasketTerms& terms, double rate, int paths, std::uint64_t seed) {
        return Unwrap(credit::NthToDefaultSpread(terms, rate, paths, seed),
                      "invalid basket terms");
      },
      nb::arg("terms"), nb::arg("rate"), nb::arg("paths"), nb::arg("seed"));
}

void BindXva(nb::module_& m) {
  using credit::ExposureProfile;
  using curves::CreditCurve;

  nb::class_<ExposureProfile>(m, "ExposureProfile")
      .def(nb::init<std::vector<double>, std::vector<double>,
                    std::vector<double>, std::vector<double>>(),
           nb::arg("times"), nb::arg("expected_exposure"),
           nb::arg("expected_negative_exposure"),
           nb::arg("potential_future_exposure"))
      .def_rw("times", &ExposureProfile::times)
      .def_rw("expected_exposure", &ExposureProfile::expected_exposure)
      .def_rw("expected_negative_exposure",
              &ExposureProfile::expected_negative_exposure)
      .def_rw("potential_future_exposure",
              &ExposureProfile::potential_future_exposure);
  m.def(
      "profile_from_paths",
      [](const std::vector<double>& times, const math::Matrix& values,
         double pfe_quantile, double threshold) {
        return Unwrap(
            credit::ProfileFromPaths(times, values, pfe_quantile, threshold),
            "need >= 2 paths, each one value per time");
      },
      nb::arg("times"), nb::arg("values"), nb::arg("pfe_quantile") = 0.975,
      nb::arg("threshold") = 1e300);
  m.def(
      "simulate_forward_exposure",
      [](double spot, double strike, double rate, double volatility,
         double maturity, int steps, int paths, std::uint64_t seed,
         double pfe_quantile, double threshold) {
        return Unwrap(credit::SimulateForwardExposure(
                          spot, strike, rate, volatility, maturity, steps,
                          paths, seed, pfe_quantile, threshold),
                      "invalid simulation inputs");
      },
      nb::arg("spot"), nb::arg("strike"), nb::arg("rate"),
      nb::arg("volatility"), nb::arg("maturity"), nb::arg("steps"),
      nb::arg("paths"), nb::arg("seed"), nb::arg("pfe_quantile") = 0.975,
      nb::arg("threshold") = 1e300);
  m.def(
      "cva",
      [](const std::vector<double>& times, const std::vector<double>& exposures,
         const CreditCurve& curve, double recovery) {
        return Unwrap(credit::Cva(times, exposures, curve, recovery), kSizes);
      },
      nb::arg("times"), nb::arg("exposures"), nb::arg("curve"),
      nb::arg("recovery"));
  m.def(
      "dva",
      [](const std::vector<double>& times,
         const std::vector<double>& negative_exposures,
         const CreditCurve& own_curve, double own_recovery) {
        return Unwrap(
            credit::Dva(times, negative_exposures, own_curve, own_recovery),
            kSizes);
      },
      nb::arg("times"), nb::arg("negative_exposures"), nb::arg("own_curve"),
      nb::arg("own_recovery"));
  m.def(
      "funding_value_adjustment",
      [](const std::vector<double>& times, const std::vector<double>& exposures,
         double funding_spread) {
        return Unwrap(
            credit::FundingValueAdjustment(times, exposures, funding_spread),
            kSizes);
      },
      nb::arg("times"), nb::arg("exposures"), nb::arg("funding_spread"));
}

}  // namespace

void BindCredit(nb::module_& m) {
  using credit::Tranche;

  nb::class_<Tranche>(m, "Tranche")
      .def(
          "__init__",
          [](Tranche* self, double attachment, double detachment) {
            new (self) Tranche{attachment, detachment};
          },
          nb::arg("attachment") = 0.0, nb::arg("detachment") = 1.0)
      .def_rw("attachment", &Tranche::attachment)
      .def_rw("detachment", &Tranche::detachment);
  m.def("tranche_loss", &credit::TrancheLoss, nb::arg("tranche"),
        nb::arg("pool_loss"));
  m.def(
      "waterfall_losses",
      [](const std::vector<Tranche>& tranches, double pool_loss) {
        return credit::WaterfallLosses(tranches, pool_loss);
      },
      nb::arg("tranches"), nb::arg("pool_loss"));

  // Chapters 9, 24 and 25.
  BindDefaultModels(m);
  BindCds(m);
  BindCopula(m);
  BindXva(m);
}

}  // namespace ofod::bindings
