// Copyright 2026 Denis Zhereshchin

#include "ofod/risk/hedging.h"

#include "ofod/stats/descriptive.h"

namespace ofod::risk {

double MinimumVarianceHedgeRatio(double correlation, double spot_vol,
                                 double futures_vol) {
  return correlation * spot_vol / futures_vol;
}

std::optional<double> HedgeRatioFromChanges(
    std::span<const double> spot_changes,
    std::span<const double> futures_changes) {
  const auto fit = stats::LinearRegression(futures_changes, spot_changes);
  if (!fit) return std::nullopt;
  return fit->slope;
}

double HedgeEffectiveness(double correlation) {
  return correlation * correlation;
}

double OptimalContracts(double hedge_ratio, double exposure_units,
                        double contract_units) {
  return hedge_ratio * exposure_units / contract_units;
}

double TailedContracts(double hedge_ratio, double exposure_value,
                       double futures_value) {
  return hedge_ratio * exposure_value / futures_value;
}

double BetaHedgeContracts(double beta, double portfolio_value,
                          double futures_value) {
  return beta * portfolio_value / futures_value;
}

double ChangeBetaContracts(double beta, double target_beta,
                           double portfolio_value, double futures_value) {
  return (target_beta - beta) * portfolio_value / futures_value;
}

double Basis(double spot, double futures) { return spot - futures; }

double EffectiveHedgePrice(double spot_at_close, double futures_at_open,
                           double futures_at_close) {
  return spot_at_close + futures_at_open - futures_at_close;
}

double ShortRollGain(std::span<const RollLeg> legs) {
  double gain = 0.0;
  for (const auto& leg : legs) gain += leg.open_price - leg.close_price;
  return gain;
}

}  // namespace ofod::risk
