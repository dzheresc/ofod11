"""Tests for the Ch 21 engines: trees, Monte Carlo, finite differences."""

import math

import pytest

from ofod import instruments
from ofod import pricers
from ofod import random

_CALL = instruments.VanillaOption(strike=40.0, maturity=0.5)
_MARKET = pricers.TreeMarket(42.0, 0.10, 0.02, 0.20)
_BSM = pricers.bsm_price(
    instruments.OptionType.CALL, 42, 40, 0.10, 0.02, 0.2, 0.5
)


@pytest.mark.parametrize("p", [1e-12, 1e-6, 0.01, 0.3, 0.5, 0.99, 1 - 1e-9])
def test_norm_inv_round_trips(p: float) -> None:
    from ofod import math as ofod_math  # noqa: PLC0415

    assert ofod_math.norm_cdf(ofod_math.norm_inv(p)) == pytest.approx(
        p, abs=1e-15
    )


def test_sobol_mean_and_errors() -> None:
    seq = random.Sobol(8)
    points = [seq.next() for _ in range(4095)]
    for d in range(8):
        assert sum(p[d] for p in points) / 4095 == pytest.approx(0.5, abs=1e-3)
    with pytest.raises(ValueError):
        random.Sobol(9)


def test_trinomial_converges() -> None:
    tri = pricers.price_trinomial(_CALL, _MARKET, 300).price
    assert tri == pytest.approx(_BSM, abs=3e-3)


@pytest.mark.parametrize("method", list(pricers.VarianceReduction))
def test_monte_carlo_methods(method: pricers.VarianceReduction) -> None:
    r = pricers.monte_carlo_european(
        _CALL, _MARKET, pricers.McOptions(paths=32_000, seed=3, method=method)
    )
    assert r.price == pytest.approx(_BSM, abs=4 * r.std_error + 1e-4)


def test_path_simulator_matches_bsm() -> None:
    r = pricers.simulate_gbm_paths(
        _MARKET,
        0.5,
        1,
        lambda path: max(path[-1] - 40.0, 0.0),
        pricers.McOptions(
            paths=20_000, method=pricers.VarianceReduction.ANTITHETIC
        ),
    )
    assert r.price == pytest.approx(_BSM, abs=4 * r.std_error)
    with pytest.raises(ValueError):
        pricers.simulate_gbm_paths(
            _MARKET,
            0.5,
            1,
            lambda path: 0.0,
            pricers.McOptions(method=pricers.VarianceReduction.STRATIFIED),
        )


def test_mix_seed_matches_splitmix64() -> None:
    assert pricers.mix_seed(0, 0) == 0xE220A8397B1DCDAF


@pytest.mark.parametrize(
    "scheme", [pricers.FdScheme.IMPLICIT, pricers.FdScheme.CRANK_NICOLSON]
)
def test_finite_differences_converge(scheme: pricers.FdScheme) -> None:
    put = instruments.VanillaOption(
        instruments.OptionType.PUT, strike=40.0, maturity=0.5
    )
    bsm = pricers.bsm_price(
        instruments.OptionType.PUT, 42, 40, 0.10, 0.02, 0.2, 0.5
    )
    grid = pricers.FdGrid(s_max=160, price_steps=200, time_steps=100)
    value = pricers.price_finite_difference(put, _MARKET, grid, scheme)
    assert value == pytest.approx(bsm, abs=1e-2)
    with pytest.raises(ValueError):
        pricers.price_finite_difference(
            put, _MARKET, pricers.FdGrid(s_max=30), scheme
        )


def test_dividend_tree_without_dividends_is_plain_tree() -> None:
    put = instruments.VanillaOption(
        instruments.OptionType.PUT, instruments.Exercise.AMERICAN, 52, 1.0
    )
    market = pricers.TreeMarket(50, 0.05, 0.0, 0.3)
    a = pricers.price_binomial_with_dividends(put, market, 60, []).price
    b = pricers.price_binomial(put, market, 60).price
    assert a == pytest.approx(b, abs=1e-12)
    assert math.isfinite(a)
