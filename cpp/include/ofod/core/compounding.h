// Copyright 2026 Denis Zhereshchin
//
// Interest rate compounding conventions (Hull, Ch 4.4).

#ifndef OFOD_CORE_COMPOUNDING_H_
#define OFOD_CORE_COMPOUNDING_H_

namespace ofod {

// How an interest rate is compounded.
class Compounding {
 public:
  enum class Kind { kContinuous, kPeriodic, kSimple };

  // Continuous compounding: growth factor exp(R * t).
  static constexpr Compounding Continuous() {
    return Compounding(Kind::kContinuous, 0);
  }
  // Compounded `frequency` times per year: growth (1 + R/m)^(m t).
  static constexpr Compounding Periodic(int frequency) {
    return Compounding(Kind::kPeriodic, frequency);
  }
  // Simple (money market) interest: growth 1 + R t.
  static constexpr Compounding Simple() {
    return Compounding(Kind::kSimple, 0);
  }

  constexpr Kind kind() const { return kind_; }
  // Compounding periods per year; only meaningful for kPeriodic.
  constexpr int frequency() const { return frequency_; }

  constexpr bool operator==(const Compounding&) const = default;

 private:
  constexpr Compounding(Kind kind, int frequency)
      : kind_(kind), frequency_(frequency) {}

  Kind kind_;
  int frequency_;
};

// Returns the amount that 1 grows to over `t` years at `rate`.
[[nodiscard]] double GrowthFactor(double rate, double t, Compounding c);

// Returns the present value of 1 paid in `t` years at `rate`.
[[nodiscard]] double DiscountFactor(double rate, double t, Compounding c);

// Returns the rate under `c` that grows 1 to `growth` over `t` years.
// Requires growth > 0 and t > 0.
[[nodiscard]] double ImpliedRate(double growth, double t, Compounding c);

// Converts `rate` expressed under `from` into the equivalent rate under `to`
// (equal growth over `t` years; `t` matters only for simple compounding).
[[nodiscard]] double EquivalentRate(double rate, Compounding from,
                                    Compounding to, double t = 1.0);

}  // namespace ofod

#endif  // OFOD_CORE_COMPOUNDING_H_
