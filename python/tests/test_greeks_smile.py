"""Greeks (Ch 19), smiles and surfaces (Ch 20) and ESOs (Ch 16)."""

import dataclasses
import math

import pytest

from ofod import curves
from ofod import greeks
from ofod import pricers
from ofod.instruments import Exercise
from ofod.instruments import OptionType
from ofod.instruments import VanillaOption

_IN = greeks.OptionInputs(
    spot=42.0, rate=0.08, yield_=0.03, volatility=0.25, maturity=0.75
)


def _bsm(option_type: OptionType, strike: float) -> greeks.greeks.Pricer:
    def pricer(x: greeks.OptionInputs) -> float:
        return pricers.bsm_price(
            option_type, x.spot, strike, x.rate, x.yield_, x.volatility,
            x.maturity,
        )  # fmt: skip

    return pricer


@pytest.mark.parametrize("option_type", [OptionType.CALL, OptionType.PUT])
def test_bumped_greeks_match_closed_forms(option_type: OptionType) -> None:
    exact = greeks.bsm_greeks(option_type, 40.0, _IN)
    bumped = greeks.bump_greeks(_bsm(option_type, 40.0), _IN)
    assert bumped.delta == pytest.approx(exact.delta, abs=1e-5)
    assert bumped.gamma == pytest.approx(exact.gamma, abs=1e-5)
    assert bumped.vega == pytest.approx(exact.vega, abs=1e-6)
    assert bumped.theta == pytest.approx(exact.theta, abs=1e-4)
    assert bumped.rho == pytest.approx(exact.rho, abs=1e-6)


def test_greeks_satisfy_the_bsm_pde() -> None:
    g = greeks.bsm_greeks(OptionType.CALL, 40.0, _IN)
    s, sig = _IN.spot, _IN.volatility
    v = _bsm(OptionType.CALL, 40.0)(_IN)
    lhs = g.theta + (_IN.rate - _IN.yield_) * s * g.delta
    lhs += 0.5 * sig * sig * s * s * g.gamma
    assert lhs == pytest.approx(_IN.rate * v, abs=1e-12)


def test_neutral_hedges() -> None:
    port = greeks.Exposure(300.0, -1200.0, 900.0)
    o1, o2 = greeks.Exposure(0.55, 0.04, 0.12), greeks.Exposure(0.3, 0.02, 0.15)
    h = greeks.gamma_vega_neutral_hedge(port, o1, o2)
    assert port.gamma + h.option1 * o1.gamma + h.option2 * o2.gamma == (
        pytest.approx(0.0, abs=1e-9)
    )
    assert port.vega + h.option1 * o1.vega + h.option2 * o2.vega == (
        pytest.approx(0.0, abs=1e-9)
    )
    with pytest.raises(ValueError):
        greeks.gamma_vega_neutral_hedge(port, o1, o1)
    with pytest.raises(ValueError):
        greeks.gamma_neutral_hedge(port, greeks.Exposure(delta=1.0))


def test_scenario_grid_is_zero_at_base() -> None:
    grid = greeks.scenario_grid(
        _bsm(OptionType.CALL, 40.0), _IN, [40.0, 42.0, 44.0], [0.2, 0.25, 0.3]
    )
    assert grid[1][1] == pytest.approx(0.0, abs=1e-14)
    assert grid[2][2] > 0.0 > grid[0][0]


def test_delta_hedge_cost_and_error_scaling() -> None:
    e = greeks.HedgeExperiment(49.0, 50.0, 0.05, 0.13, 0.2, 20 / 52)
    coarse = greeks.simulate_delta_hedge(e, 5, 20000, 3)
    fine = greeks.simulate_delta_hedge(e, 50, 20000, 3)
    se = fine.std_cost / math.sqrt(20000)
    assert fine.mean_cost == pytest.approx(fine.bsm_price, abs=4 * se)
    assert coarse.ratio / fine.ratio == pytest.approx(math.sqrt(10), rel=0.1)
    with pytest.raises(ValueError):
        greeks.simulate_delta_hedge(e, 0, 100, 1)


def _surface(
    interp: curves.VolInterpolation = curves.VolInterpolation.TOTAL_VARIANCE,
) -> curves.VolSurface:
    return curves.VolSurface(
        [0.5, 1.0],
        [90.0, 100.0, 110.0],
        [[0.25, 0.20, 0.22], [0.24, 0.21, 0.22]],
        interp,
    )


@pytest.mark.parametrize("interp", list(curves.VolInterpolation))
def test_surface_nodes_and_flat_extrapolation(
    interp: curves.VolInterpolation,
) -> None:
    s = _surface(interp)
    assert s.volatility(90.0, 0.5) == pytest.approx(0.25, abs=1e-15)
    assert s.volatility(95.0, 0.5) == pytest.approx(0.225, abs=1e-15)
    assert s.volatility(50.0, 0.1) == pytest.approx(0.25, abs=1e-15)
    assert s.volatility(200.0, 3.0) == pytest.approx(0.22, abs=1e-15)


def test_total_variance_is_linear() -> None:
    v = _surface().volatility(100.0, 0.75)
    assert v * v * 0.75 == pytest.approx(
        0.5 * (0.04 * 0.5 + 0.21**2), abs=1e-15
    )


@pytest.mark.parametrize(
    ("maturities", "strikes", "vols"),
    [
        ([1.0, 0.5], [100.0], [[0.2], [0.2]]),
        ([0.0], [100.0], [[0.2]]),
        ([1.0], [90.0, 100.0], [[0.2]]),
        ([1.0], [100.0], [[0.0]]),
        ([], [], []),
    ],
)
def test_surface_rejects_bad_grids(
    maturities: list[float], strikes: list[float], vols: list[list[float]]
) -> None:
    with pytest.raises(ValueError):
        curves.VolSurface(maturities, strikes, vols)


@pytest.mark.parametrize(
    ("option_type", "delta"),
    [(OptionType.CALL, 0.25), (OptionType.CALL, 0.5), (OptionType.PUT, -0.25)],
)
def test_strike_from_delta_round_trip(
    option_type: OptionType, delta: float
) -> None:
    x = greeks.OptionInputs(1.2, 0.03, 0.01, 0.12, 0.5)
    k = pricers.strike_from_delta(
        option_type, delta, 1.2, 0.03, 0.01, 0.12, 0.5
    )
    assert greeks.bsm_greeks(option_type, k, x).delta == pytest.approx(
        delta, abs=1e-12
    )


def test_strike_from_delta_rejects_unattainable() -> None:
    with pytest.raises(ValueError):
        pricers.strike_from_delta(OptionType.PUT, 0.1, 1.2, 0.03, 0.01, 0.1, 1)


def test_implied_density_from_flat_smile_is_lognormal() -> None:
    def call(k: float) -> float:
        return pricers.bsm_price(OptionType.CALL, 100, k, 0.05, 0, 0.2, 1)

    h, mass, k = 0.5, 0.0, 20.0
    while k < 400.0:
        g = pricers.implied_density(
            call(k - h), call(k), call(k + h), h, 0.05, 1
        )
        assert g == pytest.approx(
            pricers.lognormal_density(100, k, 0.05, 0, 0.2, 1), abs=1e-5
        )
        mass += g * h
        k += h
    assert mass == pytest.approx(1.0, abs=1e-4)


def test_binary_jump_gives_a_frown() -> None:
    def iv(k: float) -> float:
        price = pricers.binary_jump_price(
            OptionType.CALL, k, 0.0, 0.1, 58.0, 42.0, 0.5
        )
        return pricers.implied_volatility(
            OptionType.CALL, price, 50.0, k, 0.0, 0.0, 0.1
        )

    assert iv(50.0) > iv(44.0)
    assert iv(50.0) > iv(56.0)


_ESO_MARKET = pricers.TreeMarket(spot=40.0, rate=0.05, volatility=0.3)


def test_eso_without_frictions_is_european() -> None:
    eso = pricers.price_employee_stock_option(
        pricers.EsoTerms(40.0, 5.0, 0.0, 1e12, 0.0), _ESO_MARKET, 200
    )
    tree = pricers.price_binomial(
        VanillaOption(OptionType.CALL, Exercise.EUROPEAN, 40.0, 5.0),
        _ESO_MARKET,
        200,
    )
    assert eso == pytest.approx(tree.price, abs=1e-10)


def test_eso_value_falls_with_exits_and_early_exercise() -> None:
    base = pricers.EsoTerms(40.0, 10.0, 3.0, 2.0, 0.03)

    def value(t: pricers.EsoTerms) -> float:
        return pricers.price_employee_stock_option(t, _ESO_MARKET, 200)

    v = value(base)
    assert value(dataclasses.replace(base, exit_rate=0.10)) < v
    assert value(dataclasses.replace(base, exercise_multiple=1.3)) < v


@pytest.mark.parametrize(
    "terms",
    [
        pricers.EsoTerms(40.0, 10.0, 3.0, 0.9, 0.04),
        pricers.EsoTerms(40.0, 10.0, 3.0, 1.5, -0.1),
        pricers.EsoTerms(40.0, 10.0, 11.0, 1.5, 0.04),
    ],
)
def test_eso_rejects_invalid_terms(terms: pricers.EsoTerms) -> None:
    with pytest.raises(ValueError):
        pricers.price_employee_stock_option(terms, _ESO_MARKET, 10)
