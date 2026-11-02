// Copyright 2026 Denis Zhereshchin

#include "ofod/futures/treasury.h"

#include <charconv>
#include <cmath>
#include <system_error>

namespace ofod::futures {

std::optional<double> ParseThirtySeconds(std::string_view quote) {
  const auto dash = quote.find('-');
  if (dash == std::string_view::npos) return std::nullopt;
  int whole = 0;
  int ticks = 0;
  const auto head = quote.substr(0, dash);
  const auto tail = quote.substr(dash + 1);
  const auto r1 =
      std::from_chars(head.data(), head.data() + head.size(), whole);
  const auto r2 =
      std::from_chars(tail.data(), tail.data() + tail.size(), ticks);
  if (r1.ec != std::errc() || r1.ptr != head.data() + head.size() ||
      r2.ec != std::errc() || r2.ptr != tail.data() + tail.size() ||
      head.empty() || tail.empty() || ticks < 0 || ticks >= 32) {
    return std::nullopt;
  }
  return whole + ticks / 32.0;
}

double AccruedInterest(const Date& last_coupon, const Date& next_coupon,
                       const Date& settlement, double coupon_amount) {
  return coupon_amount * last_coupon.DaysUntil(settlement) /
         last_coupon.DaysUntil(next_coupon);
}

double CashPrice(double quoted_price, double accrued_interest) {
  return quoted_price + accrued_interest;
}

double ConversionFactor(double coupon_rate, double years_to_maturity) {
  constexpr double kHalfYearRate = 0.03;
  const double coupon = 100.0 * coupon_rate / 2.0;
  const int quarters = static_cast<int>(std::floor(years_to_maturity * 4.0));
  const int half_years = quarters / 2;
  // Value at the start of a coupon period of a bond with `n` half-years left.
  auto value_with = [&](int n) {
    double v = 100.0 * std::pow(1.0 + kHalfYearRate, -n);
    for (int k = 1; k <= n; ++k) {
      v += coupon * std::pow(1.0 + kHalfYearRate, -k);
    }
    return v;
  };
  if (quarters % 2 == 0) return value_with(half_years) / 100.0;
  // Odd quarter: value three months ahead (a coupon is then due), discount
  // back three months and subtract three months of accrued interest.
  const double ahead = coupon + value_with(half_years);
  const double back = ahead / std::sqrt(1.0 + kHalfYearRate);
  return (back - coupon / 2.0) / 100.0;
}

std::optional<CheapestToDeliver> FindCheapestToDeliver(
    double futures_price, std::span<const DeliverableBond> bonds) {
  std::optional<CheapestToDeliver> best;
  for (std::size_t i = 0; i < bonds.size(); ++i) {
    const double cost =
        bonds[i].quoted_price - futures_price * bonds[i].conversion_factor;
    if (!best || cost < best->cost) {
      best = CheapestToDeliver{static_cast<int>(i), cost};
    }
  }
  return best;
}

double TreasuryFuturesQuote(double cash_bond_price, double coupons_pv,
                            double rate, double t, double accrued_at_delivery,
                            double conversion_factor) {
  const double cash_futures =
      (cash_bond_price - coupons_pv) * std::exp(rate * t);
  return (cash_futures - accrued_at_delivery) / conversion_factor;
}

}  // namespace ofod::futures
