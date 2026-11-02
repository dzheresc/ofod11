// Copyright 2026 Denis Zhereshchin

#include "ofod/core/date.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace ofod {
namespace {

using Catch::Matchers::WithinAbs;

TEST_CASE("Date basics", "[core]") {
  static_assert(Date(2024, 2, 29).IsValid());
  static_assert(!Date(2023, 2, 29).IsValid());
  static_assert(Date(2024, 1, 1).DaysUntil(Date(2025, 1, 1)) == 366);
  CHECK(Date(2024, 2, 29).IsEndOfMonth());
  CHECK(Date(2024, 3, 1) > Date(2024, 2, 29));
}

TEST_CASE("Actual/360 and Actual/365F", "[core][ch06]") {
  const Date start(2026, 3, 1);
  const Date end(2026, 7, 3);  // 124 days.
  CHECK_THAT(YearFraction(start, end, DayCount::kActual360),
             WithinAbs(124.0 / 360.0, 1e-15));
  CHECK_THAT(YearFraction(start, end, DayCount::kActual365Fixed),
             WithinAbs(124.0 / 365.0, 1e-15));
}

TEST_CASE("30/360 US", "[core][ch06]") {
  // March 1 to July 3: 4 months and 2 days = 122 days.
  CHECK_THAT(
      YearFraction(Date(2026, 3, 1), Date(2026, 7, 3), DayCount::kThirty360),
      WithinAbs(122.0 / 360.0, 1e-15));
  // The 31st is treated as the 30th.
  CHECK_THAT(
      YearFraction(Date(2026, 1, 31), Date(2026, 3, 31), DayCount::kThirty360),
      WithinAbs(60.0 / 360.0, 1e-15));
  // End of February to end of February is one year.
  CHECK_THAT(
      YearFraction(Date(2023, 2, 28), Date(2024, 2, 29), DayCount::kThirty360),
      WithinAbs(1.0, 1e-15));
}

TEST_CASE("Actual/Actual ISDA splits by calendar year", "[core][ch06]") {
  // 2023-07-01 to 2024-07-01: 184 days of 2023 and 182 days of 2024.
  const double expected = 184.0 / 365.0 + 182.0 / 366.0;
  CHECK_THAT(YearFraction(Date(2023, 7, 1), Date(2024, 7, 1),
                          DayCount::kActualActualIsda),
             WithinAbs(expected, 1e-15));
  CHECK_THAT(YearFraction(Date(2024, 7, 1), Date(2023, 7, 1),
                          DayCount::kActualActualIsda),
             WithinAbs(-expected, 1e-15));
}

}  // namespace
}  // namespace ofod
