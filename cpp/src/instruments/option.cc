// Copyright 2026 Denis Zhereshchin

#include "ofod/instruments/option.h"

#include <algorithm>

namespace ofod::instruments {

double IntrinsicValue(const VanillaOption& option, double spot) {
  return option.option_type == OptionType::kCall
             ? std::max(spot - option.strike, 0.0)
             : std::max(option.strike - spot, 0.0);
}

ContractTerms AdjustForSplit(const ContractTerms& terms, int n, int m) {
  const double ratio = static_cast<double>(n) / static_cast<double>(m);
  return {terms.strike / ratio, terms.shares * ratio};
}

ContractTerms AdjustForStockDividend(const ContractTerms& terms,
                                     double fraction) {
  return {terms.strike / (1.0 + fraction), terms.shares * (1.0 + fraction)};
}

double NakedOptionMargin(OptionType option_type, double premium, double spot,
                         double strike) {
  if (option_type == OptionType::kCall) {
    const double out_of_money = std::max(strike - spot, 0.0);
    return std::max(premium + 0.2 * spot - out_of_money, premium + 0.1 * spot);
  }
  const double out_of_money = std::max(spot - strike, 0.0);
  return std::max(premium + 0.2 * spot - out_of_money, premium + 0.1 * strike);
}

}  // namespace ofod::instruments
