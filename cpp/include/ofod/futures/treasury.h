// Copyright 2026 Denis Zhereshchin
//
// Treasury bond quotes and Treasury bond futures (Hull, Ch 6.1-6.2).

#ifndef OFOD_FUTURES_TREASURY_H_
#define OFOD_FUTURES_TREASURY_H_

#include <optional>
#include <span>
#include <string_view>

#include "ofod/core/date.h"

namespace ofod::futures {

// Parses a quote in dollars and 32nds, e.g. "95-16" -> 95.5. Returns
// nullopt for malformed input or 32nds outside [0, 32).
[[nodiscard]] std::optional<double> ParseThirtySeconds(std::string_view quote);

// Accrued interest under actual/actual within the coupon period:
// coupon * days(last, settlement) / days(last, next).
[[nodiscard]] double AccruedInterest(const Date& last_coupon,
                                     const Date& next_coupon,
                                     const Date& settlement,
                                     double coupon_amount);

// Cash (dirty) price = quoted (clean) price + accrued interest.
[[nodiscard]] double CashPrice(double quoted_price, double accrued_interest);

// CME conversion factor for a bond with `coupon_rate` (annual, semiannual
// payments) and `years_to_maturity` from the first day of the delivery
// month. Maturity is rounded down to a whole number of quarters and the
// bond is valued at 6% per annum with semiannual compounding (Ch 6.2).
[[nodiscard]] double ConversionFactor(double coupon_rate,
                                      double years_to_maturity);

struct DeliverableBond {
  double quoted_price;
  double conversion_factor;
};

struct CheapestToDeliver {
  int index;    // Position of the cheapest bond in the input.
  double cost;  // Quoted price - futures price * conversion factor.
};

// Returns the cheapest-to-deliver bond; nullopt for an empty list.
[[nodiscard]] std::optional<CheapestToDeliver> FindCheapestToDeliver(
    double futures_price, std::span<const DeliverableBond> bonds);

// Quoted futures price from the CTD bond (Ch 6.2): the cash futures price
// (S - I) exp(rT), less accrued interest at delivery, divided by the
// conversion factor.
[[nodiscard]] double TreasuryFuturesQuote(double cash_bond_price,
                                          double coupons_pv, double rate,
                                          double t, double accrued_at_delivery,
                                          double conversion_factor);

}  // namespace ofod::futures

#endif  // OFOD_FUTURES_TREASURY_H_
