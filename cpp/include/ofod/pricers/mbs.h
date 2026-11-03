// Copyright 2026 Denis Zhereshchin
//
// Agency mortgage-backed securities (Hull, Ch 33.4): the PSA prepayment
// benchmark, pass-through cash flows split into interest-only (IO) and
// principal-only (PO) strips, and a Monte Carlo valuation with
// rate-dependent prepayments under the Hull-White model.

#ifndef OFOD_PRICERS_MBS_H_
#define OFOD_PRICERS_MBS_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "ofod/curves/zero_curve.h"
#include "ofod/models/short_rate.h"

namespace ofod::pricers {

// A pool of level-payment mortgages: the borrowers pay `mortgage_rate`
// (the weighted-average coupon) monthly over `months`, and the investors
// receive `pass_through_rate`; the difference is the servicing fee.
struct MortgagePool {
  double balance;
  double mortgage_rate;
  double pass_through_rate;
  int months;
  int age = 0;  // Months already elapsed, for the PSA ramp.
};

// Annual conditional prepayment rate of `speed` percent PSA in loan month
// `month` (1-based): 6% x speed / 100 x min(month / 30, 1).
[[nodiscard]] double PsaCpr(int month, double speed);

// Single monthly mortality from an annual CPR: 1 - (1 - CPR)^(1/12).
[[nodiscard]] double MonthlyMortality(double cpr);

struct MbsCashFlow {
  int month;
  double interest;   // Paid to investors.
  double scheduled;  // Scheduled principal.
  double prepaid;    // Unscheduled principal.
  double balance;    // At the end of the month.
};

// Monthly pass-through cash flows for a constant PSA speed.
[[nodiscard]] std::vector<MbsCashFlow> PassThroughCashFlows(
    const MortgagePool& pool, double psa_speed);

// Values of the pass-through and its IO and PO strips from cash flows,
// discounting month m at curve.DiscountFactor(m / 12).
struct MbsValue {
  double pass_through;
  double interest_only;
  double principal_only;
};
[[nodiscard]] MbsValue ValueCashFlows(const std::vector<MbsCashFlow>& flows,
                                      const curves::ZeroCurve& curve);

// Monthly CPR rising with the refinancing incentive: base_cpr +
// sensitivity x max(mortgage_rate - (r + spread), 0), capped at max_cpr,
// where r is the simulated short rate and spread is the mortgage spread
// over it. Seasoning follows the PSA ramp.
struct RefinancingModel {
  double base_cpr = 0.06;
  double sensitivity = 5.0;
  double spread = 0.015;
  double max_cpr = 0.6;
};

// Monte Carlo value of the pass-through and its strips with monthly
// Hull-White short rates simulated exactly (on the monthly grid) and
// prepayments from `model`; cash flows are discounted along each path.
struct MbsMcOptions {
  int paths = 5'000;
  std::uint64_t seed = 1;
};
[[nodiscard]] std::optional<MbsValue> MbsMonteCarlo(
    const MortgagePool& pool, const models::HullWhite& rates,
    const RefinancingModel& model, const MbsMcOptions& options);

}  // namespace ofod::pricers

#endif  // OFOD_PRICERS_MBS_H_
