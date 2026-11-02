// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/forward_pricer.h"

#include <cmath>

namespace ofod::pricers {

double ForwardPrice(double spot, double rate, double t) {
  return spot * std::exp(rate * t);
}

double ForwardPriceWithIncome(double spot, double income_pv, double rate,
                              double t) {
  return (spot - income_pv) * std::exp(rate * t);
}

double ForwardPriceWithYield(double spot, double rate, double yield, double t) {
  return spot * std::exp((rate - yield) * t);
}

double ForwardPriceCostOfCarry(double spot, double rate, double storage_yield,
                               double convenience_yield, double t) {
  return spot * std::exp((rate + storage_yield - convenience_yield) * t);
}

double PresentValue(std::span<const instruments::CashFlow> flows,
                    const curves::ZeroCurve& curve) {
  double pv = 0.0;
  for (const auto& flow : flows) {
    pv += flow.amount * curve.DiscountFactor(flow.time);
  }
  return pv;
}

double ForwardContractValue(double forward_price, double delivery_price,
                            double rate, double t, instruments::Side side) {
  return instruments::Sign(side) * (forward_price - delivery_price) *
         std::exp(-rate * t);
}

ForwardArbitrage DetectForwardArbitrage(double quoted_forward,
                                        double theoretical, double tolerance) {
  if (quoted_forward > theoretical + tolerance) {
    return ForwardArbitrage::kCashAndCarry;
  }
  if (quoted_forward < theoretical - tolerance) {
    return ForwardArbitrage::kReverseCashAndCarry;
  }
  return ForwardArbitrage::kNone;
}

}  // namespace ofod::pricers
