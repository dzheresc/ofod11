// Copyright 2026 Denis Zhereshchin

#include "ofod/core/compounding.h"

#include <cmath>

namespace ofod {

double GrowthFactor(double rate, double t, Compounding c) {
  switch (c.kind()) {
    case Compounding::Kind::kContinuous:
      return std::exp(rate * t);
    case Compounding::Kind::kPeriodic: {
      const double m = c.frequency();
      return std::pow(1.0 + rate / m, m * t);
    }
    case Compounding::Kind::kSimple:
      return 1.0 + rate * t;
  }
  return 0.0;  // Unreachable.
}

double DiscountFactor(double rate, double t, Compounding c) {
  return 1.0 / GrowthFactor(rate, t, c);
}

double ImpliedRate(double growth, double t, Compounding c) {
  switch (c.kind()) {
    case Compounding::Kind::kContinuous:
      return std::log(growth) / t;
    case Compounding::Kind::kPeriodic: {
      const double m = c.frequency();
      return m * (std::pow(growth, 1.0 / (m * t)) - 1.0);
    }
    case Compounding::Kind::kSimple:
      return (growth - 1.0) / t;
  }
  return 0.0;  // Unreachable.
}

double EquivalentRate(double rate, Compounding from, Compounding to, double t) {
  return ImpliedRate(GrowthFactor(rate, t, from), t, to);
}

}  // namespace ofod
