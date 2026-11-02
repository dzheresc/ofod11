// Copyright 2026 Denis Zhereshchin
//
// Calendar dates and day count conventions (Hull, Ch 4 and 6.1).

#ifndef OFOD_CORE_DATE_H_
#define OFOD_CORE_DATE_H_

#include <chrono>
#include <compare>

namespace ofod {

// A calendar date (proleptic Gregorian), backed by std::chrono.
class Date {
 public:
  constexpr Date(int year, unsigned month, unsigned day)
      : ymd_(std::chrono::year(year) / std::chrono::month(month) /
             std::chrono::day(day)) {}

  constexpr int year() const { return static_cast<int>(ymd_.year()); }
  constexpr unsigned month() const {
    return static_cast<unsigned>(ymd_.month());
  }
  constexpr unsigned day() const { return static_cast<unsigned>(ymd_.day()); }

  // Returns false for dates such as February 30.
  constexpr bool IsValid() const { return ymd_.ok(); }
  constexpr bool IsLeapYear() const { return ymd_.year().is_leap(); }
  constexpr bool IsEndOfMonth() const {
    return ymd_ == ymd_.year() / ymd_.month() / std::chrono::last;
  }

  // Returns the number of days from `this` to `other`.
  constexpr int DaysUntil(const Date& other) const {
    return static_cast<int>(
        (std::chrono::sys_days(other.ymd_) - std::chrono::sys_days(ymd_))
            .count());
  }

  constexpr auto operator<=>(const Date&) const = default;

 private:
  std::chrono::year_month_day ymd_;
};

enum class DayCount {
  kActual360,
  kActual365Fixed,
  kThirty360,         // 30/360 US (bond basis).
  kActualActualIsda,  // Actual days split by calendar year.
};

// Returns the year fraction between `start` and `end` under `convention`.
[[nodiscard]] double YearFraction(const Date& start, const Date& end,
                                  DayCount convention);

}  // namespace ofod

#endif  // OFOD_CORE_DATE_H_
