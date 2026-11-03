// Copyright 2026 Denis Zhereshchin
//
// One-factor short-rate models (Hull, Ch 31 and 32): Vasicek and CIR
// equilibrium models, and the no-arbitrage Ho-Lee and Hull-White models
// fitted to today's zero curve. Rates are instantaneous and continuously
// compounded; times are in years.

#ifndef OFOD_MODELS_SHORT_RATE_H_
#define OFOD_MODELS_SHORT_RATE_H_

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/option.h"

namespace ofod::models {

using instruments::OptionType;

// dr = a (b - r) dt + sigma dz (Ch 31.3).
struct Vasicek {
  double a;
  double b;
  double sigma;
};

// dr = a (b - r) dt + sigma sqrt(r) dz (Ch 31.3).
struct Cir {
  double a;
  double b;
  double sigma;
};

// P(t, t + tau) = A e^{-B r} (eqs. 31.6-31.8).
[[nodiscard]] double BondPrice(const Vasicek& model, double r, double tau);
[[nodiscard]] double BondPrice(const Cir& model, double r, double tau);

// Continuously compounded zero rate -ln P / tau implied by the model.
[[nodiscard]] double ZeroRate(const Vasicek& model, double r, double tau);
[[nodiscard]] double ZeroRate(const Cir& model, double r, double tau);

// European option expiring at `t` on a zero-coupon bond maturing at `s`
// with face 1 (eqs. 31.9 and 32.10): with sigma_P the volatility of
// ln P(T, s), the price is P(0, s) N(h) - K P(0, T) N(h - sigma_P) for a
// call. Shared by Vasicek and Hull-White.
[[nodiscard]] double ZeroBondOptionFormula(OptionType type, double p_expiry,
                                           double p_maturity, double strike,
                                           double sigma_p);

// Vasicek option on a zero-coupon bond, from today's short rate r.
[[nodiscard]] double ZeroBondOptionPrice(const Vasicek& model, OptionType type,
                                         double r, double expiry,
                                         double maturity, double strike);

// Vasicek parameters estimated from short rates observed every `dt` years:
// the exact AR(1) discretisation r_{i+1} = c + phi r_i + e is fitted by
// least squares, which is also the maximum likelihood estimate (Ch 31.4).
// Returns nullopt for fewer than 3 observations or phi outside (0, 1).
[[nodiscard]] std::optional<Vasicek> EstimateVasicek(
    std::span<const double> rates, double dt);

// One exact CIR transition over `dt`: r' is c times a noncentral
// chi-square with 4ab / sigma^2 degrees of freedom and noncentrality r
// e^{-a dt} / c, c = sigma^2 (1 - e^{-a dt}) / (4a), sampled as a Poisson
// mixture of gammas. Returns `steps + 1` rates starting at r0.
[[nodiscard]] std::vector<double> SimulateCir(const Cir& model, double r0,
                                              double dt, int steps,
                                              std::uint64_t seed);

// Hull-White dr = (theta(t) - a r) dt + sigma dz fitted to `curve`
// (Ch 32.3). a = 0 is the Ho-Lee model (Ch 32.2).
class HullWhite {
 public:
  // Returns nullopt unless a >= 0 and sigma > 0.
  static std::optional<HullWhite> Create(curves::ZeroCurve curve, double a,
                                         double sigma);

  [[nodiscard]] double a() const { return a_; }
  [[nodiscard]] double sigma() const { return sigma_; }
  [[nodiscard]] const curves::ZeroCurve& curve() const { return curve_; }

  // B(t, T) = (1 - e^{-a (T - t)}) / a, or T - t for Ho-Lee.
  [[nodiscard]] double B(double t, double maturity) const;

  // P(t, T) given the instantaneous short rate r at t (eq. 32.9).
  [[nodiscard]] double BondPrice(double t, double maturity, double r) const;

  // Volatility of ln P(T, s) at T: sigma_P (eq. 32.10).
  [[nodiscard]] double BondOptionVolatility(double expiry,
                                            double maturity) const;

  // European option at `expiry` on a unit zero-coupon bond maturing at
  // `maturity`.
  [[nodiscard]] double ZeroBondOption(OptionType type, double expiry,
                                      double maturity, double strike) const;

  // European option at `expiry` on a coupon bond paying amounts[i] at
  // times[i] (> expiry), by Jamshidian's decomposition into zero-coupon
  // bond options (Ch 32.3). Returns nullopt if the critical rate is not
  // found.
  [[nodiscard]] std::optional<double> CouponBondOption(
      OptionType type, double expiry, std::span<const double> times,
      std::span<const double> amounts, double strike) const;

  // Caplet (kCall) or floorlet (kPut) on the simple rate for [reset,
  // reset + accrual], as (1 + K tau) zero-coupon bond puts (calls).
  [[nodiscard]] double Caplet(OptionType type, double reset, double accrual,
                              double strike, double notional) const;

  // Instantaneous forward rate F(0, t), by central differences of the
  // curve's ln P.
  [[nodiscard]] double InstantaneousForward(double t) const;

 private:
  HullWhite(curves::ZeroCurve curve, double a, double sigma)
      : curve_(std::move(curve)), a_(a), sigma_(sigma) {}

  curves::ZeroCurve curve_;
  double a_;
  double sigma_;
};

// A market quote for calibration: a caplet with its Black price.
struct CapletQuote {
  double reset;
  double accrual;
  double strike;
  double market_price;
};

// Hull-White (a, sigma) minimising the sum of squared relative pricing
// errors over `quotes` with Nelder-Mead (Ch 32.10), started at (a0,
// sigma0). Returns nullopt if no quotes are given or the fit fails.
struct HullWhiteFit {
  double a;
  double sigma;
  double rms_relative_error;
};
[[nodiscard]] std::optional<HullWhiteFit> CalibrateHullWhite(
    const curves::ZeroCurve& curve, std::span<const CapletQuote> quotes,
    double a0 = 0.1, double sigma0 = 0.01);

}  // namespace ofod::models

#endif  // OFOD_MODELS_SHORT_RATE_H_
