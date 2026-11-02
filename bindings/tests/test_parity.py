"""Checks that ofod_cpp and ofod expose the same API and agree numerically."""

import datetime

from hypothesis import given
from hypothesis import strategies as st
import pytest

import ofod
import ofod_cpp

_SUBMODULES = ("core", "curves", "instruments", "math", "pricers")
# Typing-only names (Protocols) have no runtime counterpart in C++.
_TYPING_ONLY = {"Payoff"}


@pytest.mark.parametrize("name", _SUBMODULES)
def test_same_public_names(name: str) -> None:
    py_names = set(getattr(ofod, name).__all__)
    cpp_names = {
        n for n in dir(getattr(ofod_cpp, name)) if not n.startswith("_")
    }
    missing = py_names - cpp_names - _TYPING_ONLY
    assert not missing, f"ofod_cpp.{name} lacks {sorted(missing)}"


_rates = st.floats(min_value=-0.02, max_value=0.25)
_times = st.floats(min_value=0.05, max_value=30.0)
_freqs = st.sampled_from([0, 1, 2, 4, 12])


def _comp(lib, m: int):
    c = lib.core.Compounding
    return c.continuous() if m == 0 else c.periodic(m)


@given(_rates, _times, _freqs)
def test_growth_factor(rate: float, t: float, m: int) -> None:
    py = ofod.core.growth_factor(rate, t, _comp(ofod, m))
    cpp = ofod_cpp.core.growth_factor(rate, t, _comp(ofod_cpp, m))
    assert cpp == pytest.approx(py, rel=1e-13)


@given(
    st.floats(min_value=0.0, max_value=0.15),
    st.floats(min_value=0.5, max_value=30.0),
    st.sampled_from([1, 2, 4]),
    _rates,
)
def test_bond_price_and_duration(
    coupon: float, maturity: float, freq: int, y: float
) -> None:
    py_bond = ofod.instruments.Bond(
        coupon_rate=coupon, maturity=maturity, frequency=freq
    )
    cpp_bond = ofod_cpp.instruments.Bond(
        coupon_rate=coupon, maturity=maturity, frequency=freq
    )
    assert ofod_cpp.pricers.bond_price_from_yield(cpp_bond, y) == pytest.approx(
        ofod.pricers.bond_price_from_yield(py_bond, y), rel=1e-12
    )
    assert ofod_cpp.pricers.macaulay_duration(cpp_bond, y) == pytest.approx(
        ofod.pricers.macaulay_duration(py_bond, y), rel=1e-12
    )


@given(st.lists(_rates, min_size=1, max_size=8), _times)
def test_zero_curve(rates: list[float], t: float) -> None:
    times = [0.5 * (i + 1) for i in range(len(rates))]
    py = ofod.curves.ZeroCurve(times, rates)
    cpp = ofod_cpp.curves.ZeroCurve(times, rates)
    assert cpp.discount_factor(t) == pytest.approx(
        py.discount_factor(t), rel=1e-13
    )


def test_errors_match() -> None:
    for lib in (ofod, ofod_cpp):
        with pytest.raises(ValueError):
            lib.curves.ZeroCurve([1.0, 1.0], [0.01, 0.02])


def test_year_fraction_accepts_python_dates() -> None:
    a, b = datetime.date(2023, 7, 1), datetime.date(2024, 7, 1)
    for convention in ("ACTUAL_360", "THIRTY_360", "ACTUAL_ACTUAL_ISDA"):
        py = ofod.core.year_fraction(
            a, b, getattr(ofod.core.DayCount, convention)
        )
        cpp = ofod_cpp.core.year_fraction(
            a, b, getattr(ofod_cpp.core.DayCount, convention)
        )
        assert cpp == pytest.approx(py, abs=1e-15)


def test_positions_accept_any_callable() -> None:
    pos = ofod_cpp.instruments.Position(
        ofod_cpp.instruments.CallPayoff(340.0), quantity=100.0, premium=5.0
    )
    assert pos.profit_at(400.0) == 5500.0
    straddle_leg = ofod_cpp.instruments.Position(lambda s: abs(s - 100.0))
    assert straddle_leg.payoff_at(90.0) == 10.0
