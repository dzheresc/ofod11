import datetime

import pytest

from ofod import core

_C = core.Compounding


@pytest.mark.parametrize(
    "c", [_C.continuous(), _C.periodic(1), _C.periodic(4), _C.simple()]
)
@pytest.mark.parametrize("t", [0.25, 1.0, 7.5])
def test_implied_rate_inverts_growth(c: core.Compounding, t: float) -> None:
    g = core.growth_factor(0.0437, t, c)
    assert core.implied_rate(g, t, c) == pytest.approx(0.0437, abs=1e-14)


def test_equivalent_rate_round_trip() -> None:
    rc = core.equivalent_rate(0.1, _C.periodic(2), _C.continuous())
    back = core.equivalent_rate(rc, _C.continuous(), _C.periodic(2))
    assert back == pytest.approx(0.1, abs=1e-15)


def test_day_counts() -> None:
    d = datetime.date
    act = core.year_fraction(
        d(2026, 3, 1), d(2026, 7, 3), core.DayCount.ACTUAL_360
    )
    assert act == pytest.approx(124 / 360)
    thirty = core.year_fraction(
        d(2026, 1, 31), d(2026, 3, 31), core.DayCount.THIRTY_360
    )
    assert thirty == pytest.approx(60 / 360)
    feb = core.year_fraction(
        d(2023, 2, 28), d(2024, 2, 29), core.DayCount.THIRTY_360
    )
    assert feb == pytest.approx(1.0)
    isda = core.year_fraction(
        d(2023, 7, 1), d(2024, 7, 1), core.DayCount.ACTUAL_ACTUAL_ISDA
    )
    assert isda == pytest.approx(184 / 365 + 182 / 366)
