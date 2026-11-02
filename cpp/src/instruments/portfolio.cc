// Copyright 2026 Denis Zhereshchin

#include "ofod/instruments/portfolio.h"

#include <utility>

namespace ofod::instruments {

Portfolio& Portfolio::Add(double quantity, std::function<double(double)> payoff,
                          double premium) {
  legs_.push_back({quantity, std::move(payoff), premium});
  return *this;
}

double Portfolio::PayoffAt(double spot) const {
  double total = 0.0;
  for (const auto& leg : legs_) total += leg.quantity * leg.payoff(spot);
  return total;
}

double Portfolio::Cost() const {
  double total = 0.0;
  for (const auto& leg : legs_) total += leg.quantity * leg.premium;
  return total;
}

double Portfolio::ProfitAt(double spot) const {
  return PayoffAt(spot) - Cost();
}

}  // namespace ofod::instruments
