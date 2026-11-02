// Copyright 2026 Denis Zhereshchin
//
// Chapter 25: CDS valuation, bootstrapping a credit curve, CDS options,
// CDO tranches in the one-factor Gaussian copula, implied correlation and
// nth-to-default baskets.

#include "ofod/credit/cds.h"

#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "ofod/credit/copula.h"
#include "ofod/credit/tranche.h"
#include "ofod/curves/credit_curve.h"
#include "ofod/curves/zero_curve.h"

namespace {

namespace c = ofod::credit;
using ofod::curves::CreditCurve;
using ofod::curves::ZeroCurve;

void Cds() {
  // Hazard 2%, recovery 40%, r = 5%, annual payments (Hull, Ch 25.2).
  const c::CdsTerms terms{.maturity = 5.0, .frequency = 1, .recovery = 0.4};
  const auto credit = CreditCurve::Flat(0.02);
  const auto discount = ZeroCurve::Flat(0.05);
  const auto legs = c::CdsLegValues(terms, credit, discount);
  const auto spread = c::CdsParSpread(terms, credit, discount);
  const auto buyer = c::CdsBuyerValue(terms, 0.015, credit, discount);
  const auto upfront = c::CdsUpfront(terms, 0.01, credit, discount);
  if (!legs || !spread || !buyer || !upfront) return;
  std::cout << "5-year CDS, hazard 2%, recovery 40%, r = 5%, annual\n";
  std::cout << std::format(
      "  premium annuity {:.4f}  protection {:.6f}  par spread {:.1f} bp\n",
      legs->premium_annuity, legs->protection, 1e4 * *spread);
  std::cout << std::format(
      "  seller of a 150 bp CDS gains {:.6f}; upfront at a 100 bp coupon "
      "{:.6f}\n",
      -*buyer, *upfront);

  const std::vector<double> mats = {1, 3, 5, 7, 10};
  const std::vector<double> quotes = {0.005, 0.007, 0.009, 0.010, 0.011};
  const auto curve = c::BootstrapCreditCurve(mats, quotes, 4, 0.4, discount);
  if (!curve) return;
  std::cout << "Bootstrapped hazards (quarterly CDS, 40% recovery)\n";
  for (std::size_t i = 0; i < mats.size(); ++i) {
    const auto s =
        c::CdsParSpread({.maturity = mats[i], .frequency = 4, .recovery = 0.4},
                        *curve, discount);
    std::cout << std::format(
        "  {:4.0f}y  quote {:5.1f} bp  hazard {:.6f}  repriced {:5.1f} bp\n",
        mats[i], 1e4 * quotes[i], curve->hazards()[i], 1e4 * s.value_or(0.0));
  }
  std::cout << std::format(
      "Payer CDS option, F = 120 bp, K = 100 bp, vol 50%, 1y, annuity 3.5: "
      "{:.6f}\n",
      c::CdsOptionPrice(true, 0.012, 0.01, 0.5, 1.0, 3.5));
}

void Tranches() {
  const c::CdoPool pool{.names = 125, .hazard = 0.01, .recovery = 0.4};
  const c::CdoTerms terms{};
  const auto discount = ZeroCurve::Flat(0.035);
  const std::vector<c::Tranche> tranches = {
      {0.0, 0.03}, {0.03, 0.06}, {0.06, 0.09}, {0.09, 0.12}, {0.12, 0.22}};
  std::cout << "CDO tranche spreads (bp), 125 names, hazard 1%, R = 40%\n";
  std::cout << "  tranche    rho 0.05  rho 0.15  rho 0.30  rho 0.60\n";
  for (const auto& t : tranches) {
    const std::string label =
        std::format("{:.0f}-{:.0f}%", 100 * t.attachment, 100 * t.detachment);
    std::cout << std::format("  {:<9}", label);
    for (double rho : {0.05, 0.15, 0.30, 0.60}) {
      const auto s = c::TrancheSpread(t, pool, rho, terms, discount);
      std::cout << std::format("{:10.2f}", 1e4 * s.value_or(0.0));
    }
    std::cout << '\n';
  }
  const auto quote = c::TrancheSpread(tranches[0], pool, 0.2, terms, discount);
  if (!quote) return;
  const auto rho =
      c::ImpliedTrancheCorrelation(tranches[0], pool, *quote, terms, discount);
  std::cout << std::format(
      "Equity tranche at {:.2f} bp implies correlation {:.6f}\n", 1e4 * *quote,
      rho.value_or(0.0));
}

void Baskets() {
  std::cout << "nth-to-default, 10 names, hazard 1%, rho 0.3 (50,000 paths)\n";
  for (int nth = 1; nth <= 3; ++nth) {
    const auto r = c::NthToDefaultSpread({.nth = nth}, 0.05, 50'000, 1);
    if (!r) return;
    std::cout << std::format("  {}: {:.1f} bp (se {:.1f})\n", nth,
                             1e4 * r->spread, 1e4 * r->std_error);
  }
}

}  // namespace

int main() {
  try {
    Cds();
    Tranches();
    Baskets();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
