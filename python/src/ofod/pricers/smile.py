"""Volatility smile tools (Hull, Ch 20)."""

import math

from ofod.instruments import option as option_lib
from ofod.math import normal

OptionType = option_lib.OptionType


def strike_from_delta(
    option_type: OptionType,
    delta: float,
    spot: float,
    rate: float,
    yield_: float,
    volatility: float,
    t: float,
) -> float:
    """Strike whose BSM delta equals `delta` (a smile in delta space).

    Raises:
        ValueError: If `delta` is outside (0, e^{-qT}) for a call or
            (-e^{-qT}, 0) for a put.
    """
    scale = math.exp(yield_ * t)
    n_d1 = (
        delta * scale if option_type is OptionType.CALL else 1 + delta * scale
    )
    if not 0.0 < n_d1 < 1.0:
        raise ValueError("delta outside the attainable range")
    d1 = normal.norm_inv(n_d1)
    return spot * math.exp(
        -d1 * volatility * math.sqrt(t)
        + (rate - yield_ + 0.5 * volatility * volatility) * t
    )


def minimum_variance_delta(
    bsm_delta: float,
    bsm_vega: float,
    spot: float,
    t: float,
    a: float,
    b: float,
    c: float,
) -> float:
    """Hull and White's minimum variance delta (Ch 20.5)."""
    return bsm_delta + bsm_vega / (spot * math.sqrt(t)) * (
        a + b * bsm_delta + c * bsm_delta * bsm_delta
    )


def implied_density(
    call_low: float,
    call_mid: float,
    call_high: float,
    strike_spacing: float,
    rate: float,
    t: float,
) -> float:
    """Breeden-Litzenberger density from three calls (Ch 20 appendix)."""
    return (
        math.exp(rate * t)
        * (call_low + call_high - 2.0 * call_mid)
        / (strike_spacing * strike_spacing)
    )


def lognormal_density(
    spot: float,
    strike: float,
    rate: float,
    yield_: float,
    volatility: float,
    t: float,
) -> float:
    """BSM risk-neutral density of S_T at `strike`."""
    s = volatility * math.sqrt(t)
    m = math.log(spot) + (rate - yield_ - 0.5 * volatility * volatility) * t
    z = (math.log(strike) - m) / s
    return math.exp(-0.5 * z * z) / (strike * s * math.sqrt(2.0 * math.pi))


def binary_jump_price(
    option_type: OptionType,
    strike: float,
    rate: float,
    t: float,
    up: float,
    down: float,
    p_up: float,
) -> float:
    """Option when S_T is either `up` or `down` (Ch 20.8)."""

    def payoff(s: float) -> float:
        if option_type is OptionType.CALL:
            return max(s - strike, 0.0)
        return max(strike - s, 0.0)

    return math.exp(-rate * t) * (
        p_up * payoff(up) + (1.0 - p_up) * payoff(down)
    )
