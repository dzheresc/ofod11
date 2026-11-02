"""Day count conventions (Hull, Ch 4 and 6.1)."""

import calendar
import datetime
import enum


class DayCount(enum.Enum):
    """Supported day count conventions."""

    ACTUAL_360 = "act/360"
    ACTUAL_365_FIXED = "act/365f"
    THIRTY_360 = "30/360"
    ACTUAL_ACTUAL_ISDA = "act/act isda"


def _is_end_of_month(d: datetime.date) -> bool:
    return d.day == calendar.monthrange(d.year, d.month)[1]


def _thirty_360(start: datetime.date, end: datetime.date) -> float:
    d1, d2 = start.day, end.day
    feb_eom_start = start.month == 2 and _is_end_of_month(start)
    feb_eom_end = end.month == 2 and _is_end_of_month(end)
    if feb_eom_start and feb_eom_end:
        d2 = 30
    if feb_eom_start:
        d1 = 30
    if d2 == 31 and d1 >= 30:
        d2 = 30
    if d1 == 31:
        d1 = 30
    days = (
        360 * (end.year - start.year)
        + 30 * (end.month - start.month)
        + (d2 - d1)
    )
    return days / 360.0


def _actual_actual_isda(start: datetime.date, end: datetime.date) -> float:
    if end < start:
        return -_actual_actual_isda(end, start)
    fraction = 0.0
    cursor = start
    while cursor.year < end.year:
        next_year = datetime.date(cursor.year + 1, 1, 1)
        basis = 366.0 if calendar.isleap(cursor.year) else 365.0
        fraction += (next_year - cursor).days / basis
        cursor = next_year
    basis = 366.0 if calendar.isleap(cursor.year) else 365.0
    return fraction + (end - cursor).days / basis


def year_fraction(
    start: datetime.date, end: datetime.date, convention: DayCount
) -> float:
    """Returns the year fraction between two dates under `convention`."""
    if convention is DayCount.ACTUAL_360:
        return (end - start).days / 360.0
    if convention is DayCount.ACTUAL_365_FIXED:
        return (end - start).days / 365.0
    if convention is DayCount.THIRTY_360:
        return _thirty_360(start, end)
    return _actual_actual_isda(start, end)
