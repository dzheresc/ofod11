// Copyright 2026 Denis Zhereshchin
//
// Payoffs of the basic positions (Hull, Ch 1.3-1.5).

#ifndef OFOD_INSTRUMENTS_PAYOFF_H_
#define OFOD_INSTRUMENTS_PAYOFF_H_

#include <algorithm>
#include <concepts>

namespace ofod::instruments {

// A payoff maps the underlying price at expiry to a cash amount.
template <class P>
concept Payoff = requires(const P p, double spot) {
  { p(spot) } -> std::convertible_to<double>;
};

enum class Side { kLong, kShort };

constexpr double Sign(Side side) { return side == Side::kLong ? 1.0 : -1.0; }

// Long forward: S_T - K.
struct ForwardPayoff {
  double delivery_price;
  constexpr double operator()(double spot) const {
    return spot - delivery_price;
  }
};

// Long call: max(S_T - K, 0).
struct CallPayoff {
  double strike;
  constexpr double operator()(double spot) const {
    return std::max(spot - strike, 0.0);
  }
};

// Long put: max(K - S_T, 0).
struct PutPayoff {
  double strike;
  constexpr double operator()(double spot) const {
    return std::max(strike - spot, 0.0);
  }
};

// One unit of the underlying: S_T.
struct StockPayoff {
  constexpr double operator()(double spot) const { return spot; }
};

// A fixed amount, e.g. a zero-coupon bond's face value.
struct CashPayoff {
  double amount;
  constexpr double operator()(double /*spot*/) const { return amount; }
};

// A holding of `quantity` units of a payoff, long or short. `premium` is the
// price per unit paid (long) or received (short) at inception.
template <Payoff P>
struct Position {
  P payoff;
  Side side = Side::kLong;
  double quantity = 1.0;
  double premium = 0.0;

  // Cash received at expiry.
  constexpr double PayoffAt(double spot) const {
    return Sign(side) * quantity * payoff(spot);
  }
  // Payoff net of the premium (ignoring the time value of money, as Hull
  // does in Ch 1).
  constexpr double ProfitAt(double spot) const {
    return Sign(side) * quantity * (payoff(spot) - premium);
  }
};

}  // namespace ofod::instruments

#endif  // OFOD_INSTRUMENTS_PAYOFF_H_
