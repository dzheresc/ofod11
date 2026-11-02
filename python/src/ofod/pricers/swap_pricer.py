"""Swap valuation (Hull, Ch 7.7-7.10).

Discount factors come from a zero curve; a long swap receives fixed (IRS)
or receives the foreign currency (currency swap).
"""

from collections.abc import Sequence
import math

from ofod.curves import zero_curve
from ofod.instruments import payoff
from ofod.instruments import swap as swap_lib


def fixed_leg_bond_value(
    s: swap_lib.InterestRateSwap, curve: zero_curve.ZeroCurve
) -> float:
    """Returns B_fix, the fixed leg valued as a bond."""
    coupon = s.notional * s.fixed_rate / s.frequency
    value = sum(coupon * curve.discount_factor(t) for t in s.payment_times)
    return value + s.notional * curve.discount_factor(s.payment_times[-1])


def floating_leg_bond_value(
    s: swap_lib.InterestRateSwap, curve: zero_curve.ZeroCurve
) -> float:
    """Returns B_fl, the floating leg valued at the next reset."""
    nxt = s.notional * s.next_floating_rate / s.frequency
    return (s.notional + nxt) * curve.discount_factor(s.payment_times[0])


def swap_value(
    s: swap_lib.InterestRateSwap,
    curve: zero_curve.ZeroCurve,
    side: payoff.Side,
) -> float:
    """Returns the swap value as the difference of two bonds."""
    return payoff.sign(side) * (
        fixed_leg_bond_value(s, curve) - floating_leg_bond_value(s, curve)
    )


def swap_value_as_fras(
    s: swap_lib.InterestRateSwap,
    curve: zero_curve.ZeroCurve,
    side: payoff.Side,
) -> float:
    """Returns the swap value as a portfolio of FRAs.

    Each unknown floating rate is assumed to equal the forward rate.
    """
    tau = 1.0 / s.frequency
    times = s.payment_times
    value = (
        s.notional
        * (s.fixed_rate - s.next_floating_rate)
        * tau
        * curve.discount_factor(times[0])
    )
    for prev, t in zip(times, times[1:], strict=False):
        df_prev = curve.discount_factor(prev)
        df = curve.discount_factor(t)
        forward = (df_prev / df - 1.0) / tau
        value += s.notional * (s.fixed_rate - forward) * tau * df
    return payoff.sign(side) * value


def par_swap_rate(
    payment_times: Sequence[float],
    frequency: int,
    curve: zero_curve.ZeroCurve,
) -> float:
    """Returns the fixed rate that gives the swap zero value."""
    annuity = sum(curve.discount_factor(t) / frequency for t in payment_times)
    return (1.0 - curve.discount_factor(payment_times[-1])) / annuity


def _bond(
    notional: float,
    rate: float,
    frequency: int,
    times: Sequence[float],
    curve: zero_curve.ZeroCurve,
) -> float:
    coupon = notional * rate / frequency
    value = sum(coupon * curve.discount_factor(t) for t in times)
    return value + notional * curve.discount_factor(times[-1])


def currency_swap_value(
    s: swap_lib.CurrencySwap,
    domestic: zero_curve.ZeroCurve,
    foreign: zero_curve.ZeroCurve,
    spot: float,
    side: payoff.Side,
) -> float:
    """Returns S0 B_F - B_D; `spot` is domestic currency per foreign unit."""
    b_domestic = _bond(
        s.domestic_notional,
        s.domestic_rate,
        s.frequency,
        s.payment_times,
        domestic,
    )
    b_foreign = _bond(
        s.foreign_notional,
        s.foreign_rate,
        s.frequency,
        s.payment_times,
        foreign,
    )
    return payoff.sign(side) * (spot * b_foreign - b_domestic)


def comparative_advantage_gain(
    fixed_a: float,
    fixed_b: float,
    floating_spread_a: float,
    floating_spread_b: float,
) -> float:
    """Returns the total gain a - b available to share via a swap."""
    return math.fabs(fixed_b - fixed_a) - math.fabs(
        floating_spread_b - floating_spread_a
    )
