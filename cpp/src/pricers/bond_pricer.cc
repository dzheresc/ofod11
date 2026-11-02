// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/bond_pricer.h"

#include <cmath>

#include "ofod/math/roots.h"

namespace ofod::pricers {

using instruments::Bond;
using instruments::CashFlows;

double BondPrice(const Bond& bond, const curves::ZeroCurve& curve) {
  double price = 0.0;
  for (const auto& flow : CashFlows(bond)) {
    price += flow.amount * curve.DiscountFactor(flow.time);
  }
  return price;
}

double BondPriceFromYield(const Bond& bond, double yield,
                          Compounding compounding) {
  double price = 0.0;
  for (const auto& flow : CashFlows(bond)) {
    price += flow.amount * DiscountFactor(yield, flow.time, compounding);
  }
  return price;
}

std::optional<double> BondYield(const Bond& bond, double price,
                                Compounding compounding) {
  return math::Brent(
      [&](double y) {
        return BondPriceFromYield(bond, y, compounding) - price;
      },
      -0.5, 2.0);
}

double MacaulayDuration(const Bond& bond, double yield,
                        Compounding compounding) {
  double price = 0.0;
  double weighted = 0.0;
  for (const auto& flow : CashFlows(bond)) {
    const double pv =
        flow.amount * DiscountFactor(yield, flow.time, compounding);
    price += pv;
    weighted += flow.time * pv;
  }
  return weighted / price;
}

double ModifiedDuration(const Bond& bond, double yield,
                        Compounding compounding) {
  const double duration = MacaulayDuration(bond, yield, compounding);
  if (compounding.kind() != Compounding::Kind::kPeriodic) return duration;
  return duration / (1.0 + yield / compounding.frequency());
}

double Convexity(const Bond& bond, double yield) {
  double price = 0.0;
  double weighted = 0.0;
  for (const auto& flow : CashFlows(bond)) {
    const double pv = flow.amount * std::exp(-yield * flow.time);
    price += pv;
    weighted += flow.time * flow.time * pv;
  }
  return weighted / price;
}

double ParYield(double maturity, int frequency,
                const curves::ZeroCurve& curve) {
  const Bond annuity{.face = 1.0,
                     .coupon_rate = 1.0,
                     .maturity = maturity,
                     .frequency = frequency};
  double coupon_pv = 0.0;  // Value of 1 per year paid in coupons.
  for (const auto& flow : CashFlows(annuity)) {
    coupon_pv += curve.DiscountFactor(flow.time) / frequency;
  }
  const double final_df = curve.DiscountFactor(maturity);
  return (1.0 - final_df) / coupon_pv;
}

}  // namespace ofod::pricers
