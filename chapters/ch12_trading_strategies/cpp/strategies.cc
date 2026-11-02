// Copyright 2026 Denis Zhereshchin
//
// Chapter 12: profit tables for spreads and combinations, a principal
// protected note and payoff replication.

#include "ofod/strategies/strategies.h"

#include <format>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

void PrintProfits(std::string_view name,
                  const ofod::instruments::Portfolio& p) {
  std::cout << std::format("{:<22} cost {:>6.2f} |", name, p.Cost());
  for (double s : {20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0}) {
    std::cout << std::format(" {:>6.2f}", p.ProfitAt(s));
  }
  std::cout << '\n';
}

}  // namespace

int main() {
  namespace st = ofod::strategies;
  std::cout << std::format("{:<22} {:>11} | profit at S_T = 20, 30, ..., 80\n",
                           "Strategy", "");
  PrintProfits("Bull call 30/35", st::BullCallSpread(30, 3, 35, 1));
  PrintProfits("Bear put 30/35", st::BearPutSpread(30, 1, 35, 3));
  PrintProfits("Butterfly 55/60/65",
               st::ButterflyCallSpread(55, 10, 60, 7, 65, 5));
  PrintProfits("Straddle 70", st::Straddle(70, 4, 3));
  PrintProfits("Strangle 40/50", st::Strangle(40, 2, 50, 2));
  PrintProfits("Strip 50", st::Strip(50, 3, 2));
  PrintProfits("Strap 50", st::Strap(50, 3, 2));
  std::cout << std::format(
      "Principal protected: 1000 at 6% for 3y leaves {:.2f} for options\n",
      st::PrincipalProtectedBudget(1000.0, 0.06, 3.0));

  const std::vector<double> slopes = {0.0, 1.0, -1.0, 0.0};
  const std::vector<double> kinks = {40.0, 50.0, 60.0};
  if (const auto tent = st::ReplicatePayoff(0.0, slopes, kinks)) {
    std::cout << "Replicated tent payoff (kinks 40/50/60), legs: "
              << tent->legs().size()
              << ", payoff at 50 = " << tent->PayoffAt(50.0) << '\n';
  }
  return 0;
}
