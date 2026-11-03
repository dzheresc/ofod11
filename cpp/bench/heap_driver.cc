// Copyright 2026 Denis Zhereshchin
//
// Runs one hot path a few times without Google Benchmark, for heaptrack,
// which stops recording as soon as ofod_bench starts:
//   tools/profile/heaptrack.sh build/profile/cpp/bench/ofod_heap_driver lsm

#include <cstdio>
#include <functional>
#include <map>
#include <string>

#include "ofod/credit/xva.h"
#include "ofod/pricers/american_mc.h"
#include "ofod/pricers/binomial_tree.h"
#include "ofod/pricers/monte_carlo.h"

namespace {

using ofod::instruments::Exercise;
using ofod::instruments::OptionType;
using ofod::instruments::VanillaOption;
using ofod::pricers::TreeMarket;

const VanillaOption kPut{.option_type = OptionType::kPut,
                         .exercise = Exercise::kAmerican,
                         .strike = 50.0,
                         .maturity = 5.0 / 12.0};
const VanillaOption kCall{.strike = 40.0, .maturity = 0.5};
const TreeMarket kMarket{.spot = 50.0, .rate = 0.10, .volatility = 0.40};

const std::map<std::string, std::function<void()>> kCases = {
    {"crr",
     [] {
       (void)ofod::pricers::PriceBinomial<ofod::pricers::CoxRossRubinstein>(
           kPut, kMarket, 1024);
     }},
    {"exposure",
     [] {
       (void)ofod::credit::SimulateForwardExposure(100.0, 100.0, 0.05, 0.3, 5.0,
                                                   60, 10'000, 1);
     }},
    {"lsm",
     [] {
       (void)ofod::pricers::LongstaffSchwartz(
           OptionType::kPut, {.spot = 36.0, .rate = 0.06, .volatility = 0.2},
           40.0, 1.0, {.paths = 100'000});
     }},
    {"mc",
     [] {
       (void)ofod::pricers::MonteCarloEuropean(
           kCall, kMarket,
           {.paths = 100'000, .rng = ofod::pricers::Rng::kZiggurat});
     }},
};

}  // namespace

int main(int argc, char** argv) {
  const auto it = argc == 2 ? kCases.find(argv[1]) : kCases.end();
  if (it == kCases.end()) {
    std::fputs("usage: ofod_heap_driver crr|exposure|lsm|mc\n", stderr);
    return 2;
  }
  for (int i = 0; i < 20; ++i) it->second();
  return 0;
}
