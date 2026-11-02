// Copyright 2026 Denis Zhereshchin

#include "ofod/pricers/bounds.h"

#include <algorithm>
#include <cmath>

namespace ofod::pricers {

namespace {

double PvStrike(double strike, double rate, double t) {
  return strike * std::exp(-rate * t);
}

}  // namespace

double EuropeanCallLowerBound(double spot, double strike, double rate, double t,
                              double dividends_pv) {
  return std::max(spot - dividends_pv - PvStrike(strike, rate, t), 0.0);
}

double EuropeanPutLowerBound(double spot, double strike, double rate, double t,
                             double dividends_pv) {
  return std::max(dividends_pv + PvStrike(strike, rate, t) - spot, 0.0);
}

double CallUpperBound(double spot) { return spot; }

double EuropeanPutUpperBound(double strike, double rate, double t) {
  return PvStrike(strike, rate, t);
}

double AmericanPutUpperBound(double strike) { return strike; }

double PutFromParity(double call, double spot, double strike, double rate,
                     double t, double dividends_pv) {
  return call + PvStrike(strike, rate, t) + dividends_pv - spot;
}

double CallFromParity(double put, double spot, double strike, double rate,
                      double t, double dividends_pv) {
  return put + spot - dividends_pv - PvStrike(strike, rate, t);
}

Range AmericanCallMinusPut(double spot, double strike, double rate, double t,
                           double dividends_pv) {
  return {spot - dividends_pv - strike, spot - PvStrike(strike, rate, t)};
}

bool CallEarlyExerciseMayBeOptimal(double dividend, double strike, double rate,
                                   double time_to_next) {
  return dividend > strike * (1.0 - std::exp(-rate * time_to_next));
}

}  // namespace ofod::pricers
