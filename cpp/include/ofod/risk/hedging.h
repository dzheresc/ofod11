// Copyright 2026 Denis Zhereshchin
//
// Hedging with futures (Hull, Ch 3).

#ifndef OFOD_RISK_HEDGING_H_
#define OFOD_RISK_HEDGING_H_

#include <optional>
#include <span>

namespace ofod::risk {

// Minimum variance hedge ratio h* = rho * sigma_S / sigma_F (eq. 3.1).
[[nodiscard]] double MinimumVarianceHedgeRatio(double correlation,
                                               double spot_vol,
                                               double futures_vol);

// h* estimated as the regression slope of spot changes on futures changes.
[[nodiscard]] std::optional<double> HedgeRatioFromChanges(
    std::span<const double> spot_changes,
    std::span<const double> futures_changes);

// Hedge effectiveness: the proportion of variance removed, rho^2.
[[nodiscard]] double HedgeEffectiveness(double correlation);

// Optimal number of contracts N* = h* Q_A / Q_F (eq. 3.2), unrounded.
[[nodiscard]] double OptimalContracts(double hedge_ratio, double exposure_units,
                                      double contract_units);

// Tailed hedge N* = h* V_A / V_F using values instead of units (eq. 3.3).
[[nodiscard]] double TailedContracts(double hedge_ratio, double exposure_value,
                                     double futures_value);

// Contracts to shorten to hedge an equity portfolio: beta * V_A / V_F (3.5).
[[nodiscard]] double BetaHedgeContracts(double beta, double portfolio_value,
                                        double futures_value);

// Contracts to move a portfolio's beta from `beta` to `target_beta`;
// positive means buy futures, negative means sell.
[[nodiscard]] double ChangeBetaContracts(double beta, double target_beta,
                                         double portfolio_value,
                                         double futures_value);

// Basis b = S - F.
[[nodiscard]] double Basis(double spot, double futures);

// Effective price realised by a hedge closed at t2: S2 + F1 - F2 (Ch 3.3).
[[nodiscard]] double EffectiveHedgePrice(double spot_at_close,
                                         double futures_at_open,
                                         double futures_at_close);

// One leg of a stack-and-roll hedge (Ch 3.6): a futures position opened at
// `open_price` and closed at `close_price`.
struct RollLeg {
  double open_price;
  double close_price;
};

// Total gain per unit of a short stack-and-roll hedge: sum(open - close).
[[nodiscard]] double ShortRollGain(std::span<const RollLeg> legs);

}  // namespace ofod::risk

#endif  // OFOD_RISK_HEDGING_H_
