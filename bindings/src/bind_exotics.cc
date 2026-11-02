// Copyright 2026 Denis Zhereshchin
//
// Chapters 26 and 27 in the pricers submodule: exotic closed forms, CEV,
// jump diffusion, Heston, Longstaff-Schwartz and convertible bonds.

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

#include "bindings.h"
#include "ofod/pricers/american_mc.h"
#include "ofod/pricers/exotics.h"
#include "ofod/pricers/model_pricers.h"

namespace ofod::bindings {

namespace nb = nanobind;

namespace {

using instruments::OptionType;
using pricers::TreeMarket;

double Unwrap(std::optional<double> v, const char* message) {
  if (!v) throw nb::value_error(message);
  return *v;
}

pricers::McResult UnwrapMc(std::optional<pricers::McResult> v,
                           const char* message) {
  if (!v) throw nb::value_error(message);
  return *v;
}

constexpr const char* kSameRates = "this formula needs r != q";

void BindExoticClosedForms(nb::module_& m) {
  m.def("gap_option_price", &pricers::GapOptionPrice, nb::arg("option_type"),
        nb::arg("m"), nb::arg("strike"), nb::arg("trigger"), nb::arg("t"));
  m.def("forward_start_price", &pricers::ForwardStartPrice,
        nb::arg("option_type"), nb::arg("m"), nb::arg("t1"), nb::arg("t2"));
  m.def(
      "perpetual_american_price",
      [](OptionType type, const TreeMarket& market, double strike) {
        return Unwrap(pricers::PerpetualAmericanPrice(type, market, strike),
                      "a perpetual call needs q > 0 and a put r > 0");
      },
      nb::arg("option_type"), nb::arg("m"), nb::arg("strike"));
  m.def("cash_or_nothing_price", &pricers::CashOrNothingPrice,
        nb::arg("option_type"), nb::arg("m"), nb::arg("strike"), nb::arg("t"),
        nb::arg("cash") = 1.0);
  m.def("asset_or_nothing_price", &pricers::AssetOrNothingPrice,
        nb::arg("option_type"), nb::arg("m"), nb::arg("strike"), nb::arg("t"));

  using pricers::CompoundType;
  nb::enum_<CompoundType>(m, "CompoundType")
      .value("CALL_ON_CALL", CompoundType::kCallOnCall)
      .value("PUT_ON_CALL", CompoundType::kPutOnCall)
      .value("CALL_ON_PUT", CompoundType::kCallOnPut)
      .value("PUT_ON_PUT", CompoundType::kPutOnPut);
  m.def(
      "compound_option_price",
      [](CompoundType type, const TreeMarket& market, double k1, double t1,
         double k2, double t2) {
        return Unwrap(
            pricers::CompoundOptionPrice(type, market, k1, t1, k2, t2),
            "invalid inputs or no critical spot");
      },
      nb::arg("compound_type"), nb::arg("m"), nb::arg("k1"), nb::arg("t1"),
      nb::arg("k2"), nb::arg("t2"));
  m.def("chooser_price", &pricers::ChooserPrice, nb::arg("m"),
        nb::arg("strike"), nb::arg("t1"), nb::arg("t2"));

  using pricers::BarrierType;
  nb::enum_<BarrierType>(m, "BarrierType")
      .value("DOWN_AND_OUT", BarrierType::kDownAndOut)
      .value("DOWN_AND_IN", BarrierType::kDownAndIn)
      .value("UP_AND_OUT", BarrierType::kUpAndOut)
      .value("UP_AND_IN", BarrierType::kUpAndIn);
  m.def("barrier_option_price", &pricers::BarrierOptionPrice,
        nb::arg("barrier"), nb::arg("option_type"), nb::arg("m"),
        nb::arg("strike"), nb::arg("level"), nb::arg("t"));
  m.def(
      "floating_lookback_price",
      [](OptionType type, const TreeMarket& market, double extreme, double t) {
        return Unwrap(pricers::FloatingLookbackPrice(type, market, extreme, t),
                      kSameRates);
      },
      nb::arg("option_type"), nb::arg("m"), nb::arg("extreme"), nb::arg("t"));
  m.def(
      "fixed_lookback_price",
      [](OptionType type, const TreeMarket& market, double strike,
         double extreme, double t) {
        return Unwrap(
            pricers::FixedLookbackPrice(type, market, strike, extreme, t),
            kSameRates);
      },
      nb::arg("option_type"), nb::arg("m"), nb::arg("strike"),
      nb::arg("extreme"), nb::arg("t"));
  m.def("geometric_asian_price", &pricers::GeometricAsianPrice,
        nb::arg("option_type"), nb::arg("m"), nb::arg("strike"), nb::arg("t"));
  m.def(
      "arithmetic_asian_price",
      [](OptionType type, const TreeMarket& market, double strike, double t) {
        return Unwrap(pricers::ArithmeticAsianPrice(type, market, strike, t),
                      kSameRates);
      },
      nb::arg("option_type"), nb::arg("m"), nb::arg("strike"), nb::arg("t"));

  using pricers::ExchangeAssets;
  nb::class_<ExchangeAssets>(m, "ExchangeAssets")
      .def(
          "__init__",
          [](ExchangeAssets* self, double receive_spot, double deliver_spot,
             double receive_yield, double deliver_yield, double receive_vol,
             double deliver_vol, double correlation) {
            new (self) ExchangeAssets{
                receive_spot, deliver_spot, receive_yield, deliver_yield,
                receive_vol,  deliver_vol,  correlation};
          },
          nb::arg("receive_spot"), nb::arg("deliver_spot"),
          nb::arg("receive_yield") = 0.0, nb::arg("deliver_yield") = 0.0,
          nb::arg("receive_vol") = 0.0, nb::arg("deliver_vol") = 0.0,
          nb::arg("correlation") = 0.0)
      .def_rw("receive_spot", &ExchangeAssets::receive_spot)
      .def_rw("deliver_spot", &ExchangeAssets::deliver_spot)
      .def_rw("receive_yield", &ExchangeAssets::receive_yield)
      .def_rw("deliver_yield", &ExchangeAssets::deliver_yield)
      .def_rw("receive_vol", &ExchangeAssets::receive_vol)
      .def_rw("deliver_vol", &ExchangeAssets::deliver_vol)
      .def_rw("correlation", &ExchangeAssets::correlation);
  m.def("exchange_option_price", &pricers::ExchangeOptionPrice, nb::arg("a"),
        nb::arg("t"));
  m.def(
      "variance_swap_fair_variance",
      [](const TreeMarket& market, double t, const std::vector<double>& strikes,
         const std::function<double(double)>& implied_vol) {
        return Unwrap(
            pricers::VarianceSwapFairVariance(market, t, strikes, implied_vol),
            "need 3 or more increasing strikes around F0");
      },
      nb::arg("m"), nb::arg("t"), nb::arg("strikes"), nb::arg("implied_vol"));
}

void BindModelPricers(nb::module_& m) {
  m.def(
      "cev_price",
      [](OptionType type, const TreeMarket& market, double strike, double t,
         double alpha) {
        return Unwrap(pricers::CevPrice(type, market, strike, t, alpha),
                      "need alpha > 0 and |alpha - 1| >= 0.01");
      },
      nb::arg("option_type"), nb::arg("m"), nb::arg("strike"), nb::arg("t"),
      nb::arg("alpha"));

  using pricers::JumpParams;
  nb::class_<JumpParams>(m, "JumpParams")
      .def(nb::init<double, double, double>(), nb::arg("intensity"),
           nb::arg("mean_jump"), nb::arg("jump_vol"))
      .def_rw("intensity", &JumpParams::intensity)
      .def_rw("mean_jump", &JumpParams::mean_jump)
      .def_rw("jump_vol", &JumpParams::jump_vol);
  m.def("merton_jump_price", &pricers::MertonJumpPrice, nb::arg("option_type"),
        nb::arg("m"), nb::arg("strike"), nb::arg("t"), nb::arg("jumps"));

  using pricers::HestonParams;
  nb::class_<HestonParams>(m, "HestonParams")
      .def(nb::init<double, double, double, double, double>(), nb::arg("v0"),
           nb::arg("kappa"), nb::arg("theta"), nb::arg("sigma"), nb::arg("rho"))
      .def_rw("v0", &HestonParams::v0)
      .def_rw("kappa", &HestonParams::kappa)
      .def_rw("theta", &HestonParams::theta)
      .def_rw("sigma", &HestonParams::sigma)
      .def_rw("rho", &HestonParams::rho);
  m.def("heston_price", &pricers::HestonPrice, nb::arg("option_type"),
        nb::arg("m"), nb::arg("strike"), nb::arg("t"), nb::arg("params"));
  m.def(
      "heston_monte_carlo",
      [](OptionType type, const TreeMarket& market, double strike, double t,
         const HestonParams& params, int steps, int paths, std::uint64_t seed) {
        return UnwrapMc(pricers::HestonMonteCarlo(type, market, strike, t,
                                                  params, steps, paths, seed),
                        "invalid simulation inputs");
      },
      nb::arg("option_type"), nb::arg("m"), nb::arg("strike"), nb::arg("t"),
      nb::arg("params"), nb::arg("steps"), nb::arg("paths"), nb::arg("seed"));
}

void BindAmericanMc(nb::module_& m) {
  using pricers::LsmOptions;
  nb::class_<LsmOptions>(m, "LsmOptions")
      .def(
          "__init__",
          [](LsmOptions* self, int exercise_dates, int paths,
             std::uint64_t seed, int basis_degree) {
            new (self) LsmOptions{exercise_dates, paths, seed, basis_degree};
          },
          nb::arg("exercise_dates") = 50, nb::arg("paths") = 100'000,
          nb::arg("seed") = 1, nb::arg("basis_degree") = 2)
      .def_rw("exercise_dates", &LsmOptions::exercise_dates)
      .def_rw("paths", &LsmOptions::paths)
      .def_rw("seed", &LsmOptions::seed)
      .def_rw("basis_degree", &LsmOptions::basis_degree);
  m.def(
      "longstaff_schwartz",
      [](OptionType type, const TreeMarket& market, double strike, double t,
         const LsmOptions& options) {
        return UnwrapMc(
            pricers::LongstaffSchwartz(type, market, strike, t, options),
            "invalid Longstaff-Schwartz settings");
      },
      nb::arg("option_type"), nb::arg("m"), nb::arg("strike"), nb::arg("t"),
      nb::arg("options"));

  using pricers::ConvertibleTerms;
  nb::class_<ConvertibleTerms>(m, "ConvertibleTerms")
      .def(
          "__init__",
          [](ConvertibleTerms* self, double face, double conversion_ratio,
             double maturity, double call_price, double recovery,
             double hazard) {
            new (self) ConvertibleTerms{face,       conversion_ratio, maturity,
                                        call_price, recovery,         hazard};
          },
          nb::arg("face") = 100.0, nb::arg("conversion_ratio") = 1.0,
          nb::arg("maturity") = 1.0,
          nb::arg("call_price") = std::numeric_limits<double>::infinity(),
          nb::arg("recovery") = 0.4, nb::arg("hazard") = 0.0)
      .def_rw("face", &ConvertibleTerms::face)
      .def_rw("conversion_ratio", &ConvertibleTerms::conversion_ratio)
      .def_rw("maturity", &ConvertibleTerms::maturity)
      .def_rw("call_price", &ConvertibleTerms::call_price)
      .def_rw("recovery", &ConvertibleTerms::recovery)
      .def_rw("hazard", &ConvertibleTerms::hazard);
  m.def(
      "convertible_bond_price",
      [](const TreeMarket& market, const ConvertibleTerms& terms, int steps) {
        return Unwrap(pricers::ConvertibleBondPrice(market, terms, steps),
                      "need steps >= 1, sigma^2 > hazard and valid "
                      "probabilities");
      },
      nb::arg("m"), nb::arg("terms"), nb::arg("steps"));
}

}  // namespace

void BindExotics(nb::module_& m) {
  BindExoticClosedForms(m);
  BindModelPricers(m);
  BindAmericanMc(m);
}

}  // namespace ofod::bindings
