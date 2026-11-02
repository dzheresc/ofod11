"""Spreads, combinations and payoff replication (Hull, Ch 12).

Arguments come in (strike, premium) pairs with strikes increasing.
"""

from collections.abc import Sequence
import math

from ofod.instruments import payoff
from ofod.instruments import portfolio

Portfolio = portfolio.Portfolio
_call = payoff.CallPayoff
_put = payoff.PutPayoff


def covered_call(spot: float, strike: float, call: float) -> Portfolio:
    """Long stock bought at `spot` plus a short call."""
    return (
        Portfolio().add(1.0, payoff.StockPayoff(), spot)
        .add(-1.0, _call(strike), call)
    )  # fmt: skip


def protective_put(spot: float, strike: float, put: float) -> Portfolio:
    """Long stock plus a long put."""
    return (
        Portfolio().add(1.0, payoff.StockPayoff(), spot)
        .add(1.0, _put(strike), put)
    )  # fmt: skip


def bull_call_spread(k1: float, c1: float, k2: float, c2: float) -> Portfolio:
    """Long call K1, short call K2."""
    return Portfolio().add(1.0, _call(k1), c1).add(-1.0, _call(k2), c2)


def bull_put_spread(k1: float, p1: float, k2: float, p2: float) -> Portfolio:
    """Long put K1, short put K2."""
    return Portfolio().add(1.0, _put(k1), p1).add(-1.0, _put(k2), p2)


def bear_call_spread(k1: float, c1: float, k2: float, c2: float) -> Portfolio:
    """Short call K1, long call K2."""
    return Portfolio().add(-1.0, _call(k1), c1).add(1.0, _call(k2), c2)


def bear_put_spread(k1: float, p1: float, k2: float, p2: float) -> Portfolio:
    """Short put K1, long put K2."""
    return Portfolio().add(-1.0, _put(k1), p1).add(1.0, _put(k2), p2)


def box_spread(
    k1: float, c1: float, p1: float, k2: float, c2: float, p2: float
) -> Portfolio:
    """Bull call spread plus bear put spread; pays K2 - K1 for sure."""
    return (
        Portfolio()
        .add(1.0, _call(k1), c1)
        .add(-1.0, _call(k2), c2)
        .add(-1.0, _put(k1), p1)
        .add(1.0, _put(k2), p2)
    )


def butterfly_call_spread(
    k1: float, c1: float, k2: float, c2: float, k3: float, c3: float
) -> Portfolio:
    """Long calls K1 and K3, two short calls K2."""
    return (
        Portfolio()
        .add(1.0, _call(k1), c1)
        .add(-2.0, _call(k2), c2)
        .add(1.0, _call(k3), c3)
    )


def straddle(strike: float, call: float, put: float) -> Portfolio:
    """Long call and long put at the same strike."""
    return Portfolio().add(1.0, _call(strike), call).add(1.0, _put(strike), put)


def strip(strike: float, call: float, put: float) -> Portfolio:
    """One call and two puts."""
    return Portfolio().add(1.0, _call(strike), call).add(2.0, _put(strike), put)


def strap(strike: float, call: float, put: float) -> Portfolio:
    """Two calls and one put."""
    return Portfolio().add(2.0, _call(strike), call).add(1.0, _put(strike), put)


def strangle(
    put_strike: float, put: float, call_strike: float, call: float
) -> Portfolio:
    """Long put K1 and long call K2 > K1."""
    return (
        Portfolio()
        .add(1.0, _put(put_strike), put)
        .add(1.0, _call(call_strike), call)
    )


def principal_protected_budget(
    principal: float, rate: float, t: float
) -> float:
    """Returns P (1 - e^{-rT}), what is left for options (Ch 12.2)."""
    return principal * (1.0 - math.exp(-rate * t))


def replicate_payoff(
    value_at_zero: float, slopes: Sequence[float], kinks: Sequence[float]
) -> Portfolio:
    """Replicates a piecewise-linear payoff with cash, stock and calls.

    f(S) = value_at_zero + slopes[0] S
           + sum_i (slopes[i+1] - slopes[i]) max(S - kinks[i], 0).

    Raises:
        ValueError: Unless len(slopes) == len(kinks) + 1 and the kinks
            strictly increase.
    """
    if len(slopes) != len(kinks) + 1:
        raise ValueError("need one more slope than kinks")
    if any(b <= a for a, b in zip(kinks, kinks[1:], strict=False)):
        raise ValueError("kinks must strictly increase")
    p = Portfolio()
    if value_at_zero != 0.0:
        p.add(1.0, payoff.CashPayoff(value_at_zero))
    if slopes[0] != 0.0:
        p.add(slopes[0], payoff.StockPayoff())
    for i, k in enumerate(kinks):
        change = slopes[i + 1] - slopes[i]
        if change != 0.0:
            p.add(change, _call(k))
    return p
