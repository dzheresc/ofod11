"""Checks that ofod_cpp and ofod expose the same API and agree numerically."""

import datetime
import math

from hypothesis import given
from hypothesis import strategies as st
import pytest

import ofod
import ofod_cpp

_SUBMODULES = (
    "core",
    "credit",
    "curves",
    "futures",
    "instruments",
    "math",
    "models",
    "pricers",
    "random",
    "risk",
    "stats",
    "strategies",
)
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


@given(
    st.floats(min_value=20.0, max_value=80.0),
    st.floats(min_value=20.0, max_value=80.0),
    st.floats(min_value=0.05, max_value=0.6),
    st.sampled_from(["COX_ROSS_RUBINSTEIN", "JARROW_RUDD", "TIAN"]),
    st.sampled_from(["EUROPEAN", "AMERICAN"]),
)
def test_binomial_tree(
    spot: float, strike: float, vol: float, scheme: str, exercise: str
) -> None:
    def price(lib) -> float:
        option = lib.instruments.VanillaOption(
            lib.instruments.OptionType.PUT,
            getattr(lib.instruments.Exercise, exercise),
            strike,
            1.0,
        )
        market = lib.pricers.TreeMarket(spot, 0.05, 0.01, vol)
        scheme_ = getattr(lib.pricers.TreeScheme, scheme)
        return lib.pricers.price_binomial(option, market, 50, scheme_).price

    assert price(ofod_cpp) == pytest.approx(price(ofod), rel=1e-11, abs=1e-12)


def test_portfolio_accepts_python_payoffs() -> None:
    p = ofod_cpp.instruments.Portfolio()
    p.add(1.0, ofod_cpp.instruments.CallPayoff(50.0), 3.0).add(
        -1.0, lambda s: max(s - 60.0, 0.0), 1.0
    )
    assert p.profit_at(70.0) == 8.0
    assert len(p.legs) == 2
    for lib in (ofod, ofod_cpp):
        with pytest.raises(ValueError):
            lib.strategies.replicate_payoff(0.0, [1.0], [40.0])


@given(
    st.sampled_from(["CALL", "PUT"]),
    st.floats(min_value=20.0, max_value=80.0),
    st.floats(min_value=20.0, max_value=80.0),
    st.floats(min_value=0.05, max_value=1.0),
    st.floats(min_value=0.05, max_value=3.0),
)
def test_bsm_price(
    kind: str, spot: float, strike: float, vol: float, t: float
) -> None:
    def price(lib) -> float:
        option_type = getattr(lib.instruments.OptionType, kind)
        return lib.pricers.bsm_price(
            option_type, spot, strike, 0.04, 0.01, vol, t
        )

    assert price(ofod_cpp) == pytest.approx(price(ofod), rel=1e-12, abs=1e-13)


def test_gbm_paths_agree() -> None:
    shocks = ofod.random.NormalGenerator(4).sample(50)
    py = ofod.models.GeometricBrownianMotion(0.1, 0.3)
    cpp = ofod_cpp.models.GeometricBrownianMotion(0.1, 0.3)
    for method in ("euler_path", "exact_path"):
        a = getattr(py, method)(100.0, 0.01, shocks)
        b = getattr(cpp, method)(100.0, 0.01, shocks)
        assert b == pytest.approx(a, rel=1e-13)


def test_sobol_points_identical() -> None:
    a, b = ofod.random.Sobol(8), ofod_cpp.random.Sobol(8)
    for _ in range(300):
        assert a.next() == b.next()


@given(
    st.sampled_from(["CALL", "PUT"]),
    st.sampled_from(["EUROPEAN", "AMERICAN"]),
    st.floats(min_value=30.0, max_value=70.0),
)
def test_trees_and_finite_differences_agree(
    kind: str, exercise: str, strike: float
) -> None:
    def run(lib) -> list[float]:
        ins, p = lib.instruments, lib.pricers
        option = ins.VanillaOption(
            getattr(ins.OptionType, kind),
            getattr(ins.Exercise, exercise),
            strike,
            0.75,
        )
        market = p.TreeMarket(50.0, 0.06, 0.0, 0.3)
        grid = p.FdGrid(0.0, 200.0, 80, 40)
        divs = [ins.CashFlow(0.3, 1.0)]
        return [
            p.price_trinomial(option, market, 40).price,
            p.price_binomial_with_dividends(option, market, 40, divs).price,
            p.price_finite_difference(
                option, market, grid, p.FdScheme.CRANK_NICOLSON
            ),
            p.price_finite_difference(
                option, market, grid, p.FdScheme.EXPLICIT
            ),
        ]

    assert run(ofod_cpp) == pytest.approx(run(ofod), rel=1e-10, abs=1e-10)


def test_monte_carlo_agrees_statistically() -> None:
    option = ofod.instruments.VanillaOption(strike=40.0, maturity=0.5)
    for name in ("NONE", "ANTITHETIC", "CONTROL_VARIATE", "QUASI_RANDOM"):
        results = []
        for lib in (ofod, ofod_cpp):
            p = lib.pricers
            opt = lib.instruments.VanillaOption(strike=40.0, maturity=0.5)
            market = p.TreeMarket(42.0, 0.1, 0.0, 0.2)
            options = p.McOptions(
                paths=40_000, method=getattr(p.VarianceReduction, name)
            )
            results.append(p.monte_carlo_european(opt, market, options))
        a, b = results
        assert abs(a.price - b.price) < 5 * math.hypot(a.std_error, b.std_error)
    assert option.strike == 40.0
    assert ofod.pricers.mix_seed(7, 3) == ofod_cpp.pricers.mix_seed(7, 3)


def test_path_simulator_accepts_python_payoff() -> None:
    p = ofod_cpp.pricers
    r = p.simulate_gbm_paths(
        p.TreeMarket(42.0, 0.1, 0.0, 0.2),
        0.5,
        4,
        lambda path: max(sum(path[1:]) / 4 - 40.0, 0.0),
        p.McOptions(paths=5000, threads=4),
    )
    assert r.paths == 5000
    assert r.price > 0.0
