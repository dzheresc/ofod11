// Copyright 2026 Denis Zhereshchin
//
// Convexity, timing and quanto adjustments (Hull, Ch 30), and the
// adjustments for LIBOR-in-arrears and constant-maturity swaps (Ch 34.4).

#ifndef OFOD_PRICERS_ADJUSTMENTS_H_
#define OFOD_PRICERS_ADJUSTMENTS_H_

namespace ofod::pricers {

// Convexity adjustment to a forward bond yield y0 when the payoff is the
// yield itself (eq. 30.1): -1/2 y0^2 sigma_y^2 T G''(y0) / G'(y0), where
// G is the bond price as a function of its yield. Add it to y0.
[[nodiscard]] double ConvexityAdjustment(double forward_yield,
                                         double yield_volatility, double t,
                                         double g_first, double g_second);

// The same adjustment for a bond paying coupon_rate / frequency per period
// for `maturity` years (a whole number of periods), priced from a yield
// compounded `frequency` times a year. A zero-coupon bond has coupon 0; a
// swap rate uses coupon_rate = forward_yield (a par bond, Ch 30.1).
[[nodiscard]] double BondYieldConvexityAdjustment(double forward_yield,
                                                  double yield_volatility,
                                                  double t, double coupon_rate,
                                                  double maturity,
                                                  int frequency);

// Timing adjustment when a variable V observed at T is paid at T* = T +
// tau (eq. 30.4): E_{T*}[V_T] = E_T[V_T] exp(-rho sigma_V sigma_F F0 tau T
// / (1 + F0 tau)), where F0 is the forward rate for [T, T*] with
// compounding period tau and sigma_F its volatility. Returns the factor.
[[nodiscard]] double TimingAdjustmentFactor(double forward_rate,
                                            double forward_volatility,
                                            double variable_volatility,
                                            double correlation, double t,
                                            double tau);

// Quanto adjustment (eq. 30.6): the forward value of V measured in
// currency X but paid in currency Y grows by exp(rho sigma_W sigma_V T),
// with W the exchange rate (units of Y per unit of X) and rho the
// correlation between V and W. Returns the factor.
[[nodiscard]] double QuantoAdjustmentFactor(double correlation,
                                            double exchange_volatility,
                                            double variable_volatility,
                                            double t);

// Expected rate for a LIBOR-in-arrears payment (eq. 34.1): the forward
// rate F0 for [t, t + tau] reset and paid at t is F0 + F0^2 sigma^2 tau t /
// (1 + F0 tau).
[[nodiscard]] double InArrearsRate(double forward_rate, double volatility,
                                   double t, double tau);

// Expected constant-maturity swap rate paid at t + delay (Ch 34.4): the
// forward swap rate plus its convexity adjustment (a par bond with
// `frequency` payments over `swap_tenor` years), times the timing factor
// for paying `delay` later, using the rate `delay_rate` for the delay
// period with its volatility, correlated with the swap rate by rho.
struct CmsTerms {
  double forward_swap_rate;
  double swap_volatility;
  double t;  // Reset time of the swap rate.
  double swap_tenor;
  int frequency;
  double delay = 0.0;  // Payment delay after the reset.
  double delay_rate = 0.0;
  double delay_volatility = 0.0;
  double correlation = 1.0;
};
[[nodiscard]] double CmsRate(const CmsTerms& terms);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_ADJUSTMENTS_H_
