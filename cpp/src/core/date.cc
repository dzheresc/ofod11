// Copyright 2026 Denis Zhereshchin

#include "ofod/core/date.h"

#include <algorithm>

namespace ofod {
namespace {

double Thirty360(const Date& start, const Date& end) {
  int d1 = static_cast<int>(start.day());
  int d2 = static_cast<int>(end.day());
  // US rules: February end-of-month adjustments, then the 31st rules.
  const bool feb_eom_start = start.month() == 2 && start.IsEndOfMonth();
  const bool feb_eom_end = end.month() == 2 && end.IsEndOfMonth();
  if (feb_eom_start && feb_eom_end) d2 = 30;
  if (feb_eom_start) d1 = 30;
  if (d2 == 31 && d1 >= 30) d2 = 30;
  if (d1 == 31) d1 = 30;
  const int days =
      360 * (end.year() - start.year()) +
      30 * (static_cast<int>(end.month()) - static_cast<int>(start.month())) +
      (d2 - d1);
  return days / 360.0;
}

double ActualActualIsda(const Date& start, const Date& end) {
  if (end < start) return -ActualActualIsda(end, start);
  double fraction = 0.0;
  Date cursor = start;
  while (cursor.year() < end.year()) {
    const Date next_year(cursor.year() + 1, 1, 1);
    const double basis = cursor.IsLeapYear() ? 366.0 : 365.0;
    fraction += cursor.DaysUntil(next_year) / basis;
    cursor = next_year;
  }
  const double basis = cursor.IsLeapYear() ? 366.0 : 365.0;
  return fraction + cursor.DaysUntil(end) / basis;
}

}  // namespace

double YearFraction(const Date& start, const Date& end, DayCount convention) {
  switch (convention) {
    case DayCount::kActual360:
      return start.DaysUntil(end) / 360.0;
    case DayCount::kActual365Fixed:
      return start.DaysUntil(end) / 365.0;
    case DayCount::kThirty360:
      return Thirty360(start, end);
    case DayCount::kActualActualIsda:
      return ActualActualIsda(start, end);
  }
  return 0.0;  // Unreachable.
}

}  // namespace ofod
