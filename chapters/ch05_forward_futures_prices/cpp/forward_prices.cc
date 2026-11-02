// Copyright 2026 Denis Zhereshchin
//
// Chapter 5: forward prices with no income, known income and known yield,
// valuing a forward, and spotting arbitrage.

#include <format>
#include <iostream>
#include <string_view>
#include <vector>

#include "ofod/core/compounding.h"
#include "ofod/curves/zero_curve.h"
#include "ofod/instruments/cash_flow.h"
#include "ofod/pricers/forward_pricer.h"

namespace {

std::string_view Describe(ofod::pricers::ForwardArbitrage a) {
  switch (a) {
    case ofod::pricers::ForwardArbitrage::kNone:
      return "no arbitrage";
    case ofod::pricers::ForwardArbitrage::kCashAndCarry:
      return "buy spot, sell forward";
    case ofod::pricers::ForwardArbitrage::kReverseCashAndCarry:
      return "short spot, buy forward";
  }
  return "";
}

}  // namespace

int main() {
  namespace pricers = ofod::pricers;

  const double f0 = pricers::ForwardPrice(40.0, 0.05, 0.25);
  std::cout << std::format(
      "No income: S0 = 40, r = 5%, T = 0.25 -> F0 = {:.2f}\n", f0);
  for (double quote : {43.0, 39.0, f0}) {
    std::cout << std::format(
        "  quoted {:.2f}: {}\n", quote,
        Describe(pricers::DetectForwardArbitrage(quote, f0)));
  }

  const auto curve =
      ofod::curves::ZeroCurve::Create({1.0 / 3.0, 0.75}, {0.03, 0.04});
  if (!curve) return 1;
  const std::vector<ofod::instruments::CashFlow> coupon = {{1.0 / 3.0, 40.0}};
  const double income = pricers::PresentValue(coupon, *curve);
  std::cout << std::format(
      "Known income: bond 900, coupon 40 in 4 months -> I = {:.2f}, "
      "F0 = {:.2f}\n",
      income, pricers::ForwardPriceWithIncome(900.0, income, 0.04, 0.75));

  const double q = ofod::EquivalentRate(0.04, ofod::Compounding::Periodic(2),
                                        ofod::Compounding::Continuous());
  std::cout << std::format(
      "Known yield: 4% semiannual = {:.4f} continuous -> F0 = {:.2f}\n", q,
      pricers::ForwardPriceWithYield(25.0, 0.10, q, 0.5));

  const double f = pricers::ForwardPrice(25.0, 0.10, 0.5);
  std::cout << std::format(
      "Valuing a forward: K = 24, F0 = {:.2f} -> long value {:.2f}\n", f,
      pricers::ForwardContractValue(f, 24.0, 0.10, 0.5,
                                    ofod::instruments::Side::kLong));
  return 0;
}
