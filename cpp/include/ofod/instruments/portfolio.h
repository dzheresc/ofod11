// Copyright 2026 Denis Zhereshchin
//
// A static portfolio of payoffs held to a common expiry (Hull, Ch 12).

#ifndef OFOD_INSTRUMENTS_PORTFOLIO_H_
#define OFOD_INSTRUMENTS_PORTFOLIO_H_

#include <functional>
#include <span>
#include <vector>

namespace ofod::instruments {

// One leg: `quantity` units (negative for short) of `payoff`, each costing
// `premium` at inception.
struct Leg {
  double quantity = 1.0;
  std::function<double(double)> payoff;
  double premium = 0.0;
};

class Portfolio {
 public:
  Portfolio() = default;

  // Adds a leg and returns *this so calls can be chained.
  Portfolio& Add(double quantity, std::function<double(double)> payoff,
                 double premium = 0.0);

  // Cash received at expiry.
  [[nodiscard]] double PayoffAt(double spot) const;
  // Net cost of setting up the portfolio (negative for a net credit).
  [[nodiscard]] double Cost() const;
  // Payoff less cost, ignoring the time value of money as Hull does.
  [[nodiscard]] double ProfitAt(double spot) const;

  [[nodiscard]] std::span<const Leg> legs() const { return legs_; }

 private:
  std::vector<Leg> legs_;
};

}  // namespace ofod::instruments

#endif  // OFOD_INSTRUMENTS_PORTFOLIO_H_
