// Copyright 2026 Denis Zhereshchin
//
// Vanilla options and exchange contract mechanics (Hull, Ch 10).

#ifndef OFOD_INSTRUMENTS_OPTION_H_
#define OFOD_INSTRUMENTS_OPTION_H_

namespace ofod::instruments {

enum class OptionType { kCall, kPut };

enum class Exercise { kEuropean, kAmerican };

// A call or put on one unit of the underlying.
struct VanillaOption {
  OptionType option_type = OptionType::kCall;
  Exercise exercise = Exercise::kEuropean;
  double strike = 0.0;
  double maturity = 0.0;  // Years.
};

// max(S - K, 0) for a call, max(K - S, 0) for a put.
[[nodiscard]] double IntrinsicValue(const VanillaOption& option, double spot);

// Strike and number of shares covered by one exchange-traded contract.
struct ContractTerms {
  double strike = 0.0;
  double shares = 100.0;
};

// After an n-for-m split the strike falls to m/n of its value and the
// contract covers n/m times as many shares (Ch 10.5).
[[nodiscard]] ContractTerms AdjustForSplit(const ContractTerms& terms, int n,
                                           int m);

// A stock dividend of `fraction` (e.g. 0.25 for 25%) is treated as a
// (1 + fraction)-for-1 split.
[[nodiscard]] ContractTerms AdjustForStockDividend(const ContractTerms& terms,
                                                   double fraction);

// CBOE initial margin per unit of the underlying for a naked written option
// (Ch 10.8): the greater of premium + 20% of the spot less the amount
// out of the money, and premium + 10% of the spot (call) or strike (put).
[[nodiscard]] double NakedOptionMargin(OptionType option_type, double premium,
                                       double spot, double strike);

}  // namespace ofod::instruments

#endif  // OFOD_INSTRUMENTS_OPTION_H_
