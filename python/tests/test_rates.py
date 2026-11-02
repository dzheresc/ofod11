import math

import pytest

from ofod import core
from ofod import curves
from ofod import instruments
from ofod import pricers

_BOND = instruments.Bond(coupon_rate=0.10, maturity=3.0)


def test_cash_flows() -> None:
    flows = instruments.cash_flows(
        instruments.Bond(coupon_rate=0.06, maturity=2)
    )
    assert [f.time for f in flows] == pytest.approx([0.5, 1.0, 1.5, 2.0])
    assert flows[-1].amount == pytest.approx(103.0)
    zero = instruments.cash_flows(instruments.Bond(maturity=0.25))
    assert len(zero) == 1 and zero[0].amount == 100.0


def test_yield_inverts_price() -> None:
    for c in (core.Compounding.continuous(), core.Compounding.periodic(2)):
        price = pricers.bond_price_from_yield(_BOND, 0.07, c)
        assert pricers.bond_yield(_BOND, price, c) == pytest.approx(0.07)


def test_duration_and_convexity_finite_differences() -> None:
    y, h = 0.12, 1e-4
    b0 = pricers.bond_price_from_yield(_BOND, y)
    up = pricers.bond_price_from_yield(_BOND, y + h)
    down = pricers.bond_price_from_yield(_BOND, y - h)
    assert pricers.macaulay_duration(_BOND, y) == pytest.approx(
        -(up - down) / (2 * h) / b0, abs=1e-6
    )
    assert pricers.convexity(_BOND, y) == pytest.approx(
        (up - 2 * b0 + down) / (h * h) / b0, abs=1e-4
    )


def test_bootstrap_reprices_inputs() -> None:
    quotes = [
        curves.BondQuote(
            instruments.Bond(coupon_rate=0.05, maturity=2.0), 99.0
        ),
        curves.BondQuote(instruments.Bond(maturity=0.5), 98.0),
        curves.BondQuote(
            instruments.Bond(coupon_rate=0.04, maturity=1.0), 99.5
        ),
        curves.BondQuote(
            instruments.Bond(coupon_rate=0.06, maturity=1.5), 101.0
        ),
    ]
    curve = curves.bootstrap_from_bonds(quotes)
    for q in quotes:
        assert pricers.bond_price(q.bond, curve) == pytest.approx(q.price)
    with pytest.raises(ValueError):
        curves.bootstrap_from_bonds([quotes[0], quotes[0]])


def test_fra_at_forward_is_worth_zero() -> None:
    curve = curves.ZeroCurve([1, 2], [0.03, 0.04])
    fwd = pricers.simple_forward_rate(curve, 1, 2)
    assert fwd == pytest.approx(math.exp(0.05) - 1)
    fra = instruments.Fra(notional=1e8, fixed_rate=fwd, start=1, end=2)
    assert pricers.fra_value(
        fra, curve, instruments.Side.LONG
    ) == pytest.approx(0.0, abs=1e-6)


def test_forward_arbitrage() -> None:
    theory = pricers.forward_price(40.0, 0.05, 0.25)
    arb = pricers.ForwardArbitrage
    assert pricers.detect_forward_arbitrage(43.0, theory) is arb.CASH_AND_CARRY
    assert (
        pricers.detect_forward_arbitrage(39.0, theory)
        is arb.REVERSE_CASH_AND_CARRY
    )


def test_payoffs() -> None:
    calls = instruments.Position(
        instruments.CallPayoff(340.0), quantity=100.0, premium=5.0
    )
    assert calls.profit_at(345.0) == 0.0
    assert calls.profit_at(400.0) == 5500.0
    puts = instruments.Position(
        instruments.PutPayoff(90.0),
        side=instruments.Side.SHORT,
        quantity=100.0,
        premium=7.0,
    )
    assert puts.profit_at(80.0) == -300.0
