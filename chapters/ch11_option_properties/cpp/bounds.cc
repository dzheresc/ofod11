// Copyright 2026 Denis Zhereshchin
//
// Chapter 11: bounds on option prices, put-call parity and early exercise.

#include "ofod/pricers/bounds.h"

#include <format>
#include <iostream>

int main() {
  namespace p = ofod::pricers;
  std::cout << std::format(
      "Call lower bound (S=20, K=18, r=10%, T=1): {:.2f}\n",
      p::EuropeanCallLowerBound(20.0, 18.0, 0.10, 1.0));
  std::cout << std::format(
      "Put lower bound (S=37, K=40, r=5%, T=0.5): {:.2f}\n",
      p::EuropeanPutLowerBound(37.0, 40.0, 0.05, 0.5));
  const double parity = p::PutFromParity(3.0, 31.0, 30.0, 0.10, 0.25);
  std::cout << std::format(
      "Parity put for c=3 (S=31, K=30, r=10%, T=0.25): {:.2f}; "
      "a quoted 2.25 is too high\n",
      parity);
  const auto r = p::AmericanCallMinusPut(19.0, 20.0, 0.10, 5.0 / 12.0);
  std::cout << std::format("American: {:.2f} <= C - P <= {:.2f}\n", r.lower,
                           r.upper);
  for (double dividend : {0.5, 1.0}) {
    std::cout << std::format(
        "Dividend {:.2f} in 3 months, K=40, r=9%: early exercise {}\n",
        dividend,
        p::CallEarlyExerciseMayBeOptimal(dividend, 40.0, 0.09, 0.25)
            ? "may be optimal"
            : "never optimal");
  }
  return 0;
}
