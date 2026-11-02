// Copyright 2026 Denis Zhereshchin
//
// Black-Scholes-Merton and its relatives (Hull, Ch 15, 17, 18).
// Rates and yields are continuously compounded; times are in years.

#ifndef OFOD_PRICERS_BLACK_SCHOLES_H_
#define OFOD_PRICERS_BLACK_SCHOLES_H_

#include <optional>
#include <span>

#include "ofod/instruments/cash_flow.h"
#include "ofod/instruments/option.h"

namespace ofod::pricers {

using instruments::OptionType;

// d1 = [ln(S/K) + (r - q + sigma^2/2) T] / (sigma sqrt(T)) (eq. 17.4).
[[nodiscard]] double BsmD1(double spot, double strike, double rate,
                           double yield, double volatility, double t);
[[nodiscard]] double BsmD2(double spot, double strike, double rate,
                           double yield, double volatility, double t);

// European option on an asset paying a continuous yield q (eqs. 15.20,
// 15.21, 17.4, 17.5); q = 0 gives Black-Scholes.
[[nodiscard]] double BsmPrice(OptionType type, double spot, double strike,
                              double rate, double yield, double volatility,
                              double t);

// Currency option: q is the foreign risk-free rate (eqs. 17.11, 17.12).
[[nodiscard]] double GarmanKohlhagenPrice(OptionType type, double spot,
                                          double strike, double rate,
                                          double foreign_rate,
                                          double volatility, double t);

// Black's model for options on futures (eqs. 18.9, 18.10).
[[nodiscard]] double Black76Price(OptionType type, double futures,
                                  double strike, double rate, double volatility,
                                  double t);

// A futures-style option is Black's price without discounting (Ch 18.7).
[[nodiscard]] double FuturesStyleOptionPrice(OptionType type, double futures,
                                             double strike, double volatility,
                                             double t);

// p = c + (K - F0) e^{-rT} for European futures options (eq. 18.1).
[[nodiscard]] double FuturesPutFromParity(double call, double futures,
                                          double strike, double rate, double t);

// Volatility that reproduces `price` (Ch 15.11): Newton-Raphson on vega,
// falling back to Brent on [1e-8, 10]. Returns std::nullopt when the price
// lies outside the no-arbitrage bounds.
[[nodiscard]] std::optional<double> ImpliedVolatility(OptionType type,
                                                      double price, double spot,
                                                      double strike,
                                                      double rate, double yield,
                                                      double t);

// European option with known cash dividends: the stock price is reduced by
// the dividends' present value (Ch 15.12).
[[nodiscard]] double BsmPriceWithDividends(
    OptionType type, double spot, double strike, double rate, double volatility,
    double t, std::span<const instruments::CashFlow> dividends);

// Black's approximation for an American call on a dividend-paying stock:
// the larger of the European calls expiring at T and just before the last
// ex-dividend date (Ch 15.12).
[[nodiscard]] double BlackAmericanCallApproximation(
    double spot, double strike, double rate, double volatility, double t,
    std::span<const instruments::CashFlow> dividends);

// Warrant issued by a company with `shares` shares outstanding, `warrants`
// warrants each convertible into one share: N / (N + M) times the BSM
// call on the share (Ch 15.10).
[[nodiscard]] double WarrantPrice(double spot, double strike, double rate,
                                  double volatility, double t, double shares,
                                  double warrants);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_BLACK_SCHOLES_H_
