"""Black-Scholes-Merton and its relatives (Hull, Ch 15, 17, 18).

Rates and yields are continuously compounded; times are in years.
"""

from collections.abc import Sequence
import math

from ofod.instruments import cash_flow
from ofod.instruments import option as option_lib
from ofod.math import normal
from ofod.math import roots

OptionType = option_lib.OptionType
_MIN_VOL = 1e-8
_MAX_VOL = 10.0


def bsm_d1(
    spot: float,
    strike: float,
    rate: float,
    yield_: float,
    volatility: float,
    t: float,
) -> float:
    """Returns d1 (eq. 17.4)."""
    return (
        math.log(spot / strike) + (rate - yield_ + 0.5 * volatility**2) * t
    ) / (volatility * math.sqrt(t))


def bsm_d2(
    spot: float,
    strike: float,
    rate: float,
    yield_: float,
    volatility: float,
    t: float,
) -> float:
    """Returns d2 = d1 - sigma sqrt(T)."""
    d1 = bsm_d1(spot, strike, rate, yield_, volatility, t)
    return d1 - volatility * math.sqrt(t)


def bsm_price(
    option_type: OptionType,
    spot: float,
    strike: float,
    rate: float,
    yield_: float,
    volatility: float,
    t: float,
) -> float:
    """Returns the European price with a continuous yield q (eq. 17.4-17.5)."""
    d1 = bsm_d1(spot, strike, rate, yield_, volatility, t)
    d2 = d1 - volatility * math.sqrt(t)
    s = spot * math.exp(-yield_ * t)
    k = strike * math.exp(-rate * t)
    cdf = normal.norm_cdf
    if option_type is OptionType.CALL:
        return s * cdf(d1) - k * cdf(d2)
    return k * cdf(-d2) - s * cdf(-d1)


def garman_kohlhagen_price(
    option_type: OptionType,
    spot: float,
    strike: float,
    rate: float,
    foreign_rate: float,
    volatility: float,
    t: float,
) -> float:
    """Currency option: q is the foreign rate (eqs. 17.11-17.12)."""
    return bsm_price(
        option_type, spot, strike, rate, foreign_rate, volatility, t
    )


def black76_price(
    option_type: OptionType,
    futures: float,
    strike: float,
    rate: float,
    volatility: float,
    t: float,
) -> float:
    """Black's model for options on futures (eqs. 18.9-18.10)."""
    return bsm_price(option_type, futures, strike, rate, rate, volatility, t)


def futures_style_option_price(
    option_type: OptionType,
    futures: float,
    strike: float,
    volatility: float,
    t: float,
) -> float:
    """Black's price without discounting (Ch 18.7)."""
    return black76_price(option_type, futures, strike, 0.0, volatility, t)


def futures_put_from_parity(
    call: float, futures: float, strike: float, rate: float, t: float
) -> float:
    """Returns p = c + (K - F0) e^{-rT} (eq. 18.1)."""
    return call + (strike - futures) * math.exp(-rate * t)


def _vega(
    spot: float,
    strike: float,
    rate: float,
    yield_: float,
    volatility: float,
    t: float,
) -> float:
    d1 = bsm_d1(spot, strike, rate, yield_, volatility, t)
    return spot * math.exp(-yield_ * t) * normal.norm_pdf(d1) * math.sqrt(t)


def implied_volatility(
    option_type: OptionType,
    price: float,
    spot: float,
    strike: float,
    rate: float,
    yield_: float,
    t: float,
) -> float:
    """Returns the volatility reproducing `price` (Ch 15.11).

    Newton-Raphson on vega, falling back to Brent on [1e-8, 10].

    Raises:
        ValueError: If `price` lies outside the no-arbitrage bounds or no
            volatility reproduces it.
    """
    s = spot * math.exp(-yield_ * t)
    k = strike * math.exp(-rate * t)
    call = option_type is OptionType.CALL
    lower = max(s - k, 0.0) if call else max(k - s, 0.0)
    upper = s if call else k
    if not lower < price < upper or t <= 0.0:
        raise ValueError("price outside the no-arbitrage bounds")

    def error(vol: float) -> float:
        return (
            bsm_price(option_type, spot, strike, rate, yield_, vol, t) - price
        )

    vol = max(math.sqrt(2.0 * abs(math.log(s / k)) / t), 0.2)
    for _ in range(50):
        v = _vega(spot, strike, rate, yield_, vol, t)
        if v < 1e-12:
            break
        nxt = vol - error(vol) / v
        if not _MIN_VOL < nxt < _MAX_VOL:
            break
        if abs(nxt - vol) < 1e-14:
            return nxt
        vol = nxt
    root = roots.brent(error, _MIN_VOL, _MAX_VOL, tolerance=1e-14)
    if root is None:
        raise ValueError("no volatility reproduces the price")
    return root


def bsm_price_with_dividends(
    option_type: OptionType,
    spot: float,
    strike: float,
    rate: float,
    volatility: float,
    t: float,
    dividends: Sequence[cash_flow.CashFlow],
) -> float:
    """European price with the PV of known dividends removed (Ch 15.12)."""
    pv = sum(
        d.amount * math.exp(-rate * d.time) for d in dividends if d.time <= t
    )
    return bsm_price(option_type, spot - pv, strike, rate, 0.0, volatility, t)


def black_american_call_approximation(
    spot: float,
    strike: float,
    rate: float,
    volatility: float,
    t: float,
    dividends: Sequence[cash_flow.CashFlow],
) -> float:
    """Larger of the European calls to T and to the last ex-date (Ch 15.12)."""
    best = bsm_price_with_dividends(
        OptionType.CALL, spot, strike, rate, volatility, t, dividends
    )
    last = max((d.time for d in dividends if d.time < t), default=0.0)
    if last > 0.0:
        pv = sum(
            d.amount * math.exp(-rate * d.time)
            for d in dividends
            if d.time < last
        )
        best = max(
            best,
            bsm_price(
                OptionType.CALL, spot - pv, strike, rate, 0.0, volatility, last
            ),
        )
    return best


def warrant_price(
    spot: float,
    strike: float,
    rate: float,
    volatility: float,
    t: float,
    shares: float,
    warrants: float,
) -> float:
    """Returns N / (N + M) times the BSM call (Ch 15.10)."""
    call = bsm_price(OptionType.CALL, spot, strike, rate, 0.0, volatility, t)
    return shares / (shares + warrants) * call
