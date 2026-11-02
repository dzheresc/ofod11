"""No-arbitrage bounds and put-call parity (Hull, Ch 11).

Rates are continuously compounded; `dividends_pv` is the present value of
dividends paid during the option's life.
"""

import dataclasses
import math


def _pv_strike(strike: float, rate: float, t: float) -> float:
    return strike * math.exp(-rate * t)


def european_call_lower_bound(
    spot: float, strike: float, rate: float, t: float, dividends_pv: float = 0.0
) -> float:
    """Returns max(S - D - K e^{-rT}, 0) (eqs. 11.4, 11.8)."""
    return max(spot - dividends_pv - _pv_strike(strike, rate, t), 0.0)


def european_put_lower_bound(
    spot: float, strike: float, rate: float, t: float, dividends_pv: float = 0.0
) -> float:
    """Returns max(D + K e^{-rT} - S, 0) (eqs. 11.5, 11.9)."""
    return max(dividends_pv + _pv_strike(strike, rate, t) - spot, 0.0)


def call_upper_bound(spot: float) -> float:
    """A call is worth at most the stock (eq. 11.1)."""
    return spot


def european_put_upper_bound(strike: float, rate: float, t: float) -> float:
    """Returns K e^{-rT} (eq. 11.3)."""
    return _pv_strike(strike, rate, t)


def american_put_upper_bound(strike: float) -> float:
    """Returns K (eq. 11.2)."""
    return strike


def put_from_parity(
    call: float,
    spot: float,
    strike: float,
    rate: float,
    t: float,
    dividends_pv: float = 0.0,
) -> float:
    """Returns p = c + K e^{-rT} + D - S (eqs. 11.6, 11.10)."""
    return call + _pv_strike(strike, rate, t) + dividends_pv - spot


def call_from_parity(
    put: float,
    spot: float,
    strike: float,
    rate: float,
    t: float,
    dividends_pv: float = 0.0,
) -> float:
    """Returns c = p + S - D - K e^{-rT}."""
    return put + spot - dividends_pv - _pv_strike(strike, rate, t)


@dataclasses.dataclass(frozen=True)
class Range:
    """A closed interval."""

    lower: float
    upper: float


def american_call_minus_put(
    spot: float, strike: float, rate: float, t: float, dividends_pv: float = 0.0
) -> Range:
    """Returns S - D - K <= C - P <= S - K e^{-rT} (eqs. 11.7, 11.11)."""
    return Range(
        spot - dividends_pv - strike, spot - _pv_strike(strike, rate, t)
    )


def call_early_exercise_may_be_optimal(
    dividend: float, strike: float, rate: float, time_to_next: float
) -> bool:
    """True when D > K (1 - e^{-r tau}) (Ch 11, Technical Note 4)."""
    return dividend > strike * (1.0 - math.exp(-rate * time_to_next))
