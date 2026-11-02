"""Bond pricing, yield, duration and convexity (Hull, Ch 4.6, 4.10, 4.11)."""

import math

from ofod.core import compounding
from ofod.curves import zero_curve
from ofod.instruments import bond as bond_lib
from ofod.math import roots

_CONTINUOUS = compounding.Compounding.continuous()


def bond_price(bond: bond_lib.Bond, curve: zero_curve.ZeroCurve) -> float:
    """Returns the price of `bond` discounted on `curve`."""
    return sum(
        f.amount * curve.discount_factor(f.time)
        for f in bond_lib.cash_flows(bond)
    )


def bond_price_from_yield(
    bond: bond_lib.Bond,
    yield_: float,
    c: compounding.Compounding = _CONTINUOUS,
) -> float:
    """Returns the price of `bond` discounted at one yield under `c`."""
    return sum(
        f.amount * compounding.discount_factor(yield_, f.time, c)
        for f in bond_lib.cash_flows(bond)
    )


def bond_yield(
    bond: bond_lib.Bond,
    price: float,
    c: compounding.Compounding = _CONTINUOUS,
) -> float:
    """Returns the yield under `c` that reprices `bond` to `price`.

    Raises:
        ValueError: If no yield in [-50%, 200%] reprices the bond.
    """
    y = roots.brent(
        lambda y: bond_price_from_yield(bond, y, c) - price, -0.5, 2.0
    )
    if y is None:
        raise ValueError("no yield reprices the bond")
    return y


def macaulay_duration(
    bond: bond_lib.Bond,
    yield_: float,
    c: compounding.Compounding = _CONTINUOUS,
) -> float:
    """Returns the time-weighted average of the discounted cash flows."""
    price = 0.0
    weighted = 0.0
    for f in bond_lib.cash_flows(bond):
        pv = f.amount * compounding.discount_factor(yield_, f.time, c)
        price += pv
        weighted += f.time * pv
    return weighted / price


def modified_duration(
    bond: bond_lib.Bond, yield_: float, c: compounding.Compounding
) -> float:
    """Returns D / (1 + y/m) for a yield compounded m times a year."""
    duration = macaulay_duration(bond, yield_, c)
    if c.kind is not compounding.CompoundingKind.PERIODIC:
        return duration
    return duration / (1.0 + yield_ / c.frequency)


def convexity(bond: bond_lib.Bond, yield_: float) -> float:
    """Returns (1/B) d2B/dy2 for a continuously compounded yield."""
    price = 0.0
    weighted = 0.0
    for f in bond_lib.cash_flows(bond):
        pv = f.amount * math.exp(-yield_ * f.time)
        price += pv
        weighted += f.time * f.time * pv
    return weighted / price


def par_yield(
    maturity: float, frequency: int, curve: zero_curve.ZeroCurve
) -> float:
    """Returns the coupon rate that prices a bond at par on `curve`."""
    annuity = bond_lib.Bond(
        face=1.0, coupon_rate=1.0, maturity=maturity, frequency=frequency
    )
    coupon_pv = sum(
        curve.discount_factor(f.time) / frequency
        for f in bond_lib.cash_flows(annuity)
    )
    return (1.0 - curve.discount_factor(maturity)) / coupon_pv
