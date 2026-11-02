"""Tests for the M4 modules (Ch 14, 15, 17, 18)."""

import math

import pytest

from ofod import instruments
from ofod import models
from ofod import pricers
from ofod import random
from ofod import stats

_CALL = instruments.OptionType.CALL
_PUT = instruments.OptionType.PUT


def test_generator_is_reproducible_and_normal() -> None:
    assert random.NormalGenerator(1).sample(4) == random.NormalGenerator(
        1
    ).sample(4)
    x = random.NormalGenerator(2).sample(100_000)
    assert stats.mean(x) == pytest.approx(0.0, abs=0.01)
    assert stats.variance(x) == pytest.approx(1.0, abs=0.01)


def test_cholesky_rejects_bad_matrices() -> None:
    with pytest.raises(ValueError):
        random.cholesky([[1.0, 2.0], [2.0, 1.0]])
    with pytest.raises(ValueError):
        random.cholesky([[1.0, 0.5], [0.4, 1.0]])
    with pytest.raises(ValueError):
        random.cholesky([[1.0, 0.5]])


def test_quadratic_variation_tends_to_t() -> None:
    n = 100_000
    w = models.ArithmeticBrownianMotion(0.0, 1.0)
    path = w.path(0.0, 1.5 / n, random.NormalGenerator(3).sample(n))
    assert models.quadratic_variation(path) == pytest.approx(1.5, abs=0.03)


@pytest.mark.parametrize("vol", [0.05, 0.2, 0.6, 1.5])
@pytest.mark.parametrize("kind", [_CALL, _PUT])
def test_implied_vol_round_trips(
    vol: float, kind: instruments.OptionType
) -> None:
    price = pricers.bsm_price(kind, 100, 110, 0.03, 0.01, vol, 0.75)
    iv = pricers.implied_volatility(kind, price, 100, 110, 0.03, 0.01, 0.75)
    assert iv == pytest.approx(vol, abs=1e-9)


def test_implied_vol_rejects_arbitrage_prices() -> None:
    with pytest.raises(ValueError):
        pricers.implied_volatility(_CALL, 101.0, 100, 90, 0.05, 0.0, 1.0)


def test_black_equals_bsm_on_forward() -> None:
    r, t, s = 0.05, 0.8, 40.0
    f = s * math.exp(r * t)
    assert pricers.black76_price(_CALL, f, 42, r, 0.25, t) == pytest.approx(
        pricers.bsm_price(_CALL, s, 42, r, 0.0, 0.25, t), abs=1e-12
    )


def test_futures_tree_converges_to_black() -> None:
    put = instruments.VanillaOption(_PUT, strike=20.0, maturity=1 / 3)
    market = pricers.TreeMarket(20.0, 0.09, 0.09, 0.25)
    tree = pricers.price_binomial(put, market, 1000).price
    black = pricers.black76_price(_PUT, 20.0, 20.0, 0.09, 0.25, 1 / 3)
    assert tree == pytest.approx(black, abs=2e-3)
