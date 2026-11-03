"""Convexity, timing and quanto adjustments (Hull, Ch 30 and 34.4)."""

import dataclasses
import math


def convexity_adjustment(
    forward_yield: float,
    yield_volatility: float,
    t: float,
    g_first: float,
    g_second: float,
) -> float:
    """Returns -1/2 y0^2 sigma^2 T G''/G' to add to y0 (eq. 30.1)."""
    return (
        -0.5 * forward_yield**2 * yield_volatility**2 * t * g_second / g_first
    )


def bond_yield_convexity_adjustment(
    forward_yield: float,
    yield_volatility: float,
    t: float,
    coupon_rate: float,
    maturity: float,
    frequency: int,
) -> float:
    """Returns the adjustment for a bond priced from its yield.

    The bond pays coupon_rate / frequency per period for `maturity` years
    and its yield is compounded `frequency` times a year; a swap rate uses
    coupon_rate = forward_yield.
    """
    m = float(frequency)
    n = round(maturity * m)
    base = 1.0 + forward_yield / m
    g1 = 0.0
    g2 = 0.0
    for i in range(1, n + 1):
        cash = coupon_rate / m + (1.0 if i == n else 0.0)
        g1 += cash * -i / m * math.pow(base, -i - 1)
        g2 += cash * i * (i + 1) / (m * m) * math.pow(base, -i - 2)
    return convexity_adjustment(forward_yield, yield_volatility, t, g1, g2)


def timing_adjustment_factor(
    forward_rate: float,
    forward_volatility: float,
    variable_volatility: float,
    correlation: float,
    t: float,
    tau: float,
) -> float:
    """Returns exp(-rho sigma_V sigma_F F0 tau T / (1 + F0 tau)) (30.4)."""
    return math.exp(
        -correlation
        * variable_volatility
        * forward_volatility
        * forward_rate
        * tau
        * t
        / (1.0 + forward_rate * tau)
    )


def quanto_adjustment_factor(
    correlation: float,
    exchange_volatility: float,
    variable_volatility: float,
    t: float,
) -> float:
    """Returns exp(rho sigma_W sigma_V T) (eq. 30.6)."""
    return math.exp(correlation * exchange_volatility * variable_volatility * t)


def in_arrears_rate(
    forward_rate: float, volatility: float, t: float, tau: float
) -> float:
    """Returns F0 + F0^2 sigma^2 tau t / (1 + F0 tau) (eq. 34.1)."""
    return forward_rate + forward_rate**2 * volatility**2 * tau * t / (
        1.0 + forward_rate * tau
    )


@dataclasses.dataclass(frozen=True)
class CmsTerms:
    """A constant-maturity swap rate set at t and paid `delay` later."""

    forward_swap_rate: float
    swap_volatility: float
    t: float
    swap_tenor: float
    frequency: int
    delay: float = 0.0
    delay_rate: float = 0.0
    delay_volatility: float = 0.0
    correlation: float = 1.0


def cms_rate(terms: CmsTerms) -> float:
    """Returns the convexity- and timing-adjusted CMS rate (Ch 34.4)."""
    adjusted = terms.forward_swap_rate + bond_yield_convexity_adjustment(
        terms.forward_swap_rate,
        terms.swap_volatility,
        terms.t,
        terms.forward_swap_rate,
        terms.swap_tenor,
        terms.frequency,
    )
    if terms.delay <= 0.0:
        return adjusted
    return adjusted * timing_adjustment_factor(
        terms.delay_rate,
        terms.delay_volatility,
        terms.swap_volatility,
        terms.correlation,
        terms.t,
        terms.delay,
    )
