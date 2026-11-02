"""FRA valuation (Hull, Ch 4.9)."""

from ofod.curves import zero_curve
from ofod.instruments import fra as fra_lib
from ofod.instruments import payoff


def simple_forward_rate(
    curve: zero_curve.ZeroCurve, t1: float, t2: float
) -> float:
    """Returns the forward rate for [t1, t2], simply compounded."""
    return (curve.discount_factor(t1) / curve.discount_factor(t2) - 1.0) / (
        t2 - t1
    )


def fra_value(
    fra: fra_lib.Fra, curve: zero_curve.ZeroCurve, side: payoff.Side
) -> float:
    """Returns the value of `fra` today.

    LONG receives the fixed rate and pays floating; SHORT pays fixed. The
    forward rate is assumed realised and the payment at `end` is discounted
    on `curve`.
    """
    tau = fra.end - fra.start
    forward = simple_forward_rate(curve, fra.start, fra.end)
    value = (
        fra.notional
        * (fra.fixed_rate - forward)
        * tau
        * curve.discount_factor(fra.end)
    )
    return payoff.sign(side) * value
