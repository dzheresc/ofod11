"""Nonstandard swaps (Hull, Ch 34).

Values are to the receiver of the fixed leg and assume forward rates are
realised after convexity and timing adjustments.
"""

from collections.abc import Sequence
import dataclasses
import math

from ofod.curves import zero_curve
from ofod.math import normal
from ofod.pricers import adjustments
from ofod.pricers import rate_options
from ofod.pricers.fra_pricer import simple_forward_rate

ZeroCurve = zero_curve.ZeroCurve


@dataclasses.dataclass(frozen=True)
class SwapPeriod:
    """The rate for [start, end] is set at start and paid at end."""

    start: float
    end: float
    notional: float
    fixed_rate: float


@dataclasses.dataclass(frozen=True)
class CmsLeg:
    """The swap rate paid by a CMS leg and its timing inputs."""

    cms_tenor: float
    cms_frequency: int
    forward_vol: float
    correlation: float = 1.0


def swap_schedule(
    start: float,
    maturity: float,
    frequency: int,
    notional: float,
    fixed_rate: float,
    amortization: float = 0.0,
    step_up: float = 0.0,
) -> list[SwapPeriod]:
    """Returns a regular schedule with amortizing notional and step-ups."""
    tau = 1.0 / frequency
    n = round((maturity - start) * frequency)
    return [
        SwapPeriod(
            start + i * tau,
            start + (i + 1) * tau,
            notional - i * amortization,
            fixed_rate + i * step_up,
        )
        for i in range(n)
    ]


def schedule_swap_value(
    curve: ZeroCurve, periods: Sequence[SwapPeriod]
) -> float:
    """Returns sum N tau (K - F) P(0, end)."""
    return sum(
        p.notional
        * (p.end - p.start)
        * (p.fixed_rate - simple_forward_rate(curve, p.start, p.end))
        * curve.discount_factor(p.end)
        for p in periods
    )


def par_fixed_rate(curve: ZeroCurve, periods: Sequence[SwapPeriod]) -> float:
    """Returns the fixed rate giving the schedule a zero value."""
    floating = 0.0
    annuity = 0.0
    for p in periods:
        tau = p.end - p.start
        df = curve.discount_factor(p.end)
        floating += (
            p.notional * tau * simple_forward_rate(curve, p.start, p.end) * df
        )
        annuity += p.notional * tau * df
    return floating / annuity


def in_arrears_swap_value(
    curve: ZeroCurve, periods: Sequence[SwapPeriod], vols: Sequence[float]
) -> float:
    """Returns a LIBOR-in-arrears swap value (eq. 34.1)."""
    value = 0.0
    for p, vol in zip(periods, vols, strict=False):
        tau = p.end - p.start
        forward = simple_forward_rate(curve, p.end, p.end + tau)
        rate = adjustments.in_arrears_rate(forward, vol, p.end, tau)
        value += (
            p.notional
            * tau
            * (p.fixed_rate - rate)
            * curve.discount_factor(p.end)
        )
    return value


def cms_swap_value(
    curve: ZeroCurve,
    periods: Sequence[SwapPeriod],
    leg: CmsLeg,
    swap_vols: Sequence[float],
) -> float:
    """Returns a constant maturity swap value (Ch 34.4)."""
    value = 0.0
    for p, vol in zip(periods, swap_vols, strict=False):
        tau = p.end - p.start
        swap = rate_options.forward_swap_rate(
            curve, p.start, leg.cms_tenor, leg.cms_frequency
        )
        terms = adjustments.CmsTerms(
            forward_swap_rate=swap.rate,
            swap_volatility=vol,
            t=p.start,
            swap_tenor=leg.cms_tenor,
            frequency=leg.cms_frequency,
            delay=tau,
            delay_rate=simple_forward_rate(curve, p.start, p.end),
            delay_volatility=leg.forward_vol,
            correlation=leg.correlation,
        )
        value += (
            p.notional
            * tau
            * (p.fixed_rate - adjustments.cms_rate(terms))
            * curve.discount_factor(p.end)
        )
    return value


def _probability_above(
    forward: float, strike: float, vol: float, t: float
) -> float:
    if strike <= 0.0:
        return 1.0
    sd = vol * math.sqrt(t)
    if sd <= 0.0:
        return 1.0 if forward > strike else 0.0
    return normal.norm_cdf(math.log(forward / strike) / sd - 0.5 * sd)


def accrual_fixed_leg_value(
    curve: ZeroCurve,
    periods: Sequence[SwapPeriod],
    low: float,
    high: float,
    vol: float,
    observations: int,
) -> float:
    """Returns an accrual swap's fixed leg (Ch 34.6)."""
    value = 0.0
    for p in periods:
        tau = p.end - p.start
        in_range = 0.0
        for k in range(1, observations + 1):
            t = p.start + tau * k / observations
            forward = simple_forward_rate(curve, t, t + tau)
            in_range += _probability_above(
                forward, low, vol, t
            ) - _probability_above(forward, high, vol, t)
        value += (
            p.notional
            * p.fixed_rate
            * tau
            * in_range
            / observations
            * curve.discount_factor(p.end)
        )
    return value
