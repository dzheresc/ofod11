// Copyright 2026 Denis Zhereshchin
//
// Chapter 6: Treasury bond quotes, conversion factors, cheapest-to-deliver,
// rate futures convexity adjustment and duration-based hedging.

#include "ofod/futures/rate_futures.h"

#include <format>
#include <iostream>
#include <vector>

#include "ofod/core/date.h"
#include "ofod/futures/treasury.h"

int main() {
  namespace futures = ofod::futures;
  const auto quoted = futures::ParseThirtySeconds("95-16");
  if (!quoted) return 1;
  const double accrued =
      futures::AccruedInterest(ofod::Date(2026, 1, 10), ofod::Date(2026, 7, 10),
                               ofod::Date(2026, 3, 5), 5.5);
  std::cout << std::format("95-16 = {:.4f}, accrued {:.4f}, cash {:.4f}\n",
                           *quoted, accrued,
                           futures::CashPrice(*quoted, accrued));

  std::cout << std::format(
      "Conversion factors: 10% 20y2m {:.4f}, 8% 18y4m {:.4f}\n",
      futures::ConversionFactor(0.10, 20.0 + 2.0 / 12.0),
      futures::ConversionFactor(0.08, 18.0 + 4.0 / 12.0));

  const std::vector<futures::DeliverableBond> bonds = {
      {99.50, 1.0382}, {143.50, 1.5188}, {119.75, 1.2615}};
  const auto ctd = futures::FindCheapestToDeliver(93.25, bonds);
  if (!ctd) return 1;
  std::cout << std::format("Cheapest to deliver: bond {} (cost {:.2f})\n",
                           ctd->index + 1, ctd->cost);

  std::cout << std::format(
      "Convexity adjustment, sigma 1.2%, 8y: {:.4f}%\n",
      100.0 * futures::ConvexityAdjustment(0.012, 8.0, 8.25));
  std::cout << std::format(
      "Duration hedge: {:.2f} contracts\n",
      futures::DurationHedgeContracts(10'000'000.0, 6.8, 93'062.5, 9.2));
  return 0;
}
