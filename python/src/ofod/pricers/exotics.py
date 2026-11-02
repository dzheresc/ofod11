"""Closed forms for exotic options (Hull, Ch 26).

Markets are spot, continuously compounded r and yield q and a flat
volatility; barriers and lookbacks assume continuous monitoring.
"""

from collections.abc import Callable
from collections.abc import Sequence
import dataclasses
import enum
import math

from ofod.instruments.option import OptionType
from ofod.math import normal
from ofod.math import roots
from ofod.pricers import binomial_tree
from ofod.pricers import black_scholes

TreeMarket = binomial_tree.TreeMarket
_n = normal.norm_cdf
_m = normal.bivariate_norm_cdf


def _bsm(
    option_type: OptionType, m: TreeMarket, spot: float, strike: float, t: float
) -> float:
    return black_scholes.bsm_price(
        option_type, spot, strike, m.rate, m.yield_, m.volatility, t
    )


def _sign(option_type: OptionType) -> float:
    return 1.0 if option_type is OptionType.CALL else -1.0


def gap_option_price(
    option_type: OptionType,
    m: TreeMarket,
    strike: float,
    trigger: float,
    t: float,
) -> float:
    """Pays S - K1 if S > K2 (call), K1 - S if S < K2 (put) (Ch 26.5)."""
    d1 = black_scholes.bsm_d1(
        m.spot, trigger, m.rate, m.yield_, m.volatility, t
    )
    d2 = d1 - m.volatility * math.sqrt(t)
    phi = _sign(option_type)
    return phi * (
        m.spot * math.exp(-m.yield_ * t) * _n(phi * d1)
        - strike * math.exp(-m.rate * t) * _n(phi * d2)
    )


def forward_start_price(
    option_type: OptionType, m: TreeMarket, t1: float, t2: float
) -> float:
    """At-the-money option starting at t1, expiring at t2 (Ch 26.6)."""
    return math.exp(-m.yield_ * t1) * _bsm(
        option_type, m, m.spot, m.spot, t2 - t1
    )


def perpetual_american_price(
    option_type: OptionType, m: TreeMarket, strike: float
) -> float:
    """Perpetual American option (Ch 26.4).

    Raises:
        ValueError: For a call with q <= 0 or a put with r <= 0.
    """
    s2 = m.volatility**2
    w = m.rate - m.yield_ - 0.5 * s2
    root = math.sqrt(w * w + 2.0 * s2 * m.rate)
    if option_type is OptionType.CALL:
        if not m.yield_ > 0.0:
            raise ValueError("a perpetual call needs q > 0")
        a = (-w + root) / s2
        if m.spot >= a * strike / (a - 1.0):
            return m.spot - strike
        return strike / (a - 1.0) * math.pow((a - 1.0) / a * m.spot / strike, a)
    if not m.rate > 0.0:
        raise ValueError("a perpetual put needs r > 0")
    a = (-w - root) / s2
    if m.spot <= a * strike / (a - 1.0):
        return strike - m.spot
    return strike / (1.0 - a) * math.pow((a - 1.0) / a * m.spot / strike, a)


def cash_or_nothing_price(
    option_type: OptionType,
    m: TreeMarket,
    strike: float,
    t: float,
    cash: float = 1.0,
) -> float:
    """Pays `cash` if the option ends in the money (Ch 26.10)."""
    d2 = black_scholes.bsm_d2(m.spot, strike, m.rate, m.yield_, m.volatility, t)
    return cash * math.exp(-m.rate * t) * _n(_sign(option_type) * d2)


def asset_or_nothing_price(
    option_type: OptionType, m: TreeMarket, strike: float, t: float
) -> float:
    """Pays S_T if the option ends in the money (Ch 26.10)."""
    d1 = black_scholes.bsm_d1(m.spot, strike, m.rate, m.yield_, m.volatility, t)
    return m.spot * math.exp(-m.yield_ * t) * _n(_sign(option_type) * d1)


class CompoundType(enum.Enum):
    """The four compound options."""

    CALL_ON_CALL = "call_on_call"
    PUT_ON_CALL = "put_on_call"
    CALL_ON_PUT = "call_on_put"
    PUT_ON_PUT = "put_on_put"


def compound_option_price(
    compound_type: CompoundType,
    m: TreeMarket,
    k1: float,
    t1: float,
    k2: float,
    t2: float,
) -> float:
    """Geske's compound option (Ch 26.8).

    Raises:
        ValueError: For invalid inputs or an unbracketed critical spot.
    """
    if not (t2 > t1 > 0.0 and k1 > 0.0 and k2 > 0.0):
        raise ValueError("need t2 > t1 > 0 and positive strikes")
    on_call = compound_type in (
        CompoundType.CALL_ON_CALL,
        CompoundType.PUT_ON_CALL,
    )
    underlying = OptionType.CALL if on_call else OptionType.PUT
    tau = t2 - t1

    def gap(s: float) -> float:
        return _bsm(underlying, m, s, k2, tau) - k1

    lo, hi = 1e-10 * k2, k2
    if on_call:
        while gap(hi) < 0.0 and hi < 1e12 * k2:
            hi *= 2.0
    else:
        if gap(lo) <= 0.0:
            raise ValueError("the put is never worth k1")
        while gap(hi) > 0.0 and hi < 1e12 * k2:
            hi *= 2.0
    critical = roots.brent(gap, lo, hi, tolerance=1e-12)
    if critical is None:
        raise ValueError("no critical spot")
    v = m.volatility
    carry = m.rate - m.yield_ + 0.5 * v * v
    a1 = (math.log(m.spot / critical) + carry * t1) / (v * math.sqrt(t1))
    a2 = a1 - v * math.sqrt(t1)
    b1 = (math.log(m.spot / k2) + carry * t2) / (v * math.sqrt(t2))
    b2 = b1 - v * math.sqrt(t2)
    rho = math.sqrt(t1 / t2)
    s = m.spot * math.exp(-m.yield_ * t2)
    k = k2 * math.exp(-m.rate * t2)
    pay = k1 * math.exp(-m.rate * t1)
    if compound_type is CompoundType.CALL_ON_CALL:
        return s * _m(a1, b1, rho) - k * _m(a2, b2, rho) - pay * _n(a2)
    if compound_type is CompoundType.PUT_ON_CALL:
        return k * _m(-a2, b2, -rho) - s * _m(-a1, b1, -rho) + pay * _n(-a2)
    if compound_type is CompoundType.CALL_ON_PUT:
        return k * _m(-a2, -b2, rho) - s * _m(-a1, -b1, rho) - pay * _n(-a2)
    return s * _m(a1, -b1, -rho) - k * _m(a2, -b2, -rho) + pay * _n(a2)


def chooser_price(m: TreeMarket, strike: float, t1: float, t2: float) -> float:
    """Simple chooser: call or put (strike K, expiry t2) chosen at t1."""
    tau = t2 - t1
    put_strike = strike * math.exp(-(m.rate - m.yield_) * tau)
    return _bsm(OptionType.CALL, m, m.spot, strike, t2) + math.exp(
        -m.yield_ * tau
    ) * _bsm(OptionType.PUT, m, m.spot, put_strike, t1)


class BarrierType(enum.Enum):
    """Knock-out and knock-in barriers below or above the spot."""

    DOWN_AND_OUT = "down_and_out"
    DOWN_AND_IN = "down_and_in"
    UP_AND_OUT = "up_and_out"
    UP_AND_IN = "up_and_in"


def barrier_option_price(
    barrier: BarrierType,
    option_type: OptionType,
    m: TreeMarket,
    strike: float,
    level: float,
    t: float,
) -> float:
    """Barrier option without rebate (Ch 26.9, Reiner-Rubinstein)."""
    down = barrier in (BarrierType.DOWN_AND_OUT, BarrierType.DOWN_AND_IN)
    knock_in = barrier in (BarrierType.DOWN_AND_IN, BarrierType.UP_AND_IN)
    vanilla = _bsm(option_type, m, m.spot, strike, t)
    if (m.spot <= level) if down else (m.spot >= level):
        return vanilla if knock_in else 0.0
    s, v = m.spot, m.volatility
    vt = v * math.sqrt(t)
    mu = (m.rate - m.yield_ - 0.5 * v * v) / (v * v)
    phi = _sign(option_type)
    eta = 1.0 if down else -1.0
    carry = s * math.exp(-m.yield_ * t)
    disc = strike * math.exp(-m.rate * t)
    x1 = math.log(s / strike) / vt + (1.0 + mu) * vt
    x2 = math.log(s / level) / vt + (1.0 + mu) * vt
    y1 = math.log(level * level / (s * strike)) / vt + (1.0 + mu) * vt
    y2 = math.log(level / s) / vt + (1.0 + mu) * vt
    hs = level / s

    def plain(x: float) -> float:
        return phi * carry * _n(phi * x) - phi * disc * _n(phi * (x - vt))

    def reflected(y: float) -> float:
        return phi * carry * math.pow(hs, 2.0 * (mu + 1.0)) * _n(eta * y) - (
            phi * disc * math.pow(hs, 2.0 * mu) * _n(eta * (y - vt))
        )

    a, b, c, d = plain(x1), plain(x2), reflected(y1), reflected(y2)
    above = strike > level
    call = option_type is OptionType.CALL
    if down and call:
        value_in = c if above else a - b + d
    elif call:
        value_in = a if above else b - c + d
    elif down:
        value_in = b - c + d if above else a
    else:
        value_in = a - b + d if above else c
    return value_in if knock_in else vanilla - value_in


def floating_lookback_price(
    option_type: OptionType, m: TreeMarket, extreme: float, t: float
) -> float:
    """Floating lookback; `extreme` is S_min (call) or S_max (put).

    Raises:
        ValueError: If r == q.
    """
    b = m.rate - m.yield_
    if b == 0.0:
        raise ValueError("lookback formulas need r != q")
    v = m.volatility
    vt = v * math.sqrt(t)
    ratio = v * v / (2.0 * b)
    s = m.spot * math.exp(-m.yield_ * t)
    disc = extreme * math.exp(-m.rate * t)
    if option_type is OptionType.CALL:
        lg = math.log(m.spot / extreme)
        a1 = (lg + (b + 0.5 * v * v) * t) / vt
        a2 = a1 - vt
        a3 = (lg + (-b + 0.5 * v * v) * t) / vt
        y1 = -2.0 * (b - 0.5 * v * v) * lg / (v * v)
        return (
            s * _n(a1)
            - s * ratio * _n(-a1)
            - disc * (_n(a2) - ratio * math.exp(y1) * _n(-a3))
        )
    lg = math.log(extreme / m.spot)
    b1 = (lg + (-b + 0.5 * v * v) * t) / vt
    b2 = b1 - vt
    b3 = (lg + (b - 0.5 * v * v) * t) / vt
    y2 = 2.0 * (b - 0.5 * v * v) * lg / (v * v)
    return (
        disc * (_n(b1) - ratio * math.exp(y2) * _n(-b3))
        + s * ratio * _n(-b2)
        - s * _n(b2)
    )


def fixed_lookback_price(
    option_type: OptionType,
    m: TreeMarket,
    strike: float,
    extreme: float,
    t: float,
) -> float:
    """Fixed lookback; `extreme` is S_max (call) or S_min (put).

    Raises:
        ValueError: If r == q.
    """
    s = m.spot * math.exp(-m.yield_ * t)
    k = strike * math.exp(-m.rate * t)
    if option_type is OptionType.CALL:
        put = floating_lookback_price(
            OptionType.PUT, m, max(extreme, strike), t
        )
        return put + s - k
    call = floating_lookback_price(OptionType.CALL, m, min(extreme, strike), t)
    return call + k - s


def geometric_asian_price(
    option_type: OptionType, m: TreeMarket, strike: float, t: float
) -> float:
    """Option on the continuous geometric average (Ch 26.13)."""
    v = m.volatility
    q = 0.5 * (m.rate + m.yield_ + v * v / 6.0)
    return black_scholes.bsm_price(
        option_type, m.spot, strike, m.rate, q, v / math.sqrt(3.0), t
    )


def arithmetic_asian_price(
    option_type: OptionType, m: TreeMarket, strike: float, t: float
) -> float:
    """Arithmetic average by moment matching (eqs. 26.3, 26.4).

    Raises:
        ValueError: If r == q.
    """
    b = m.rate - m.yield_
    if b == 0.0:
        raise ValueError("moment matching needs r != q")
    s2, s0 = m.volatility**2, m.spot
    m1 = (math.exp(b * t) - 1.0) / (b * t) * s0
    m2 = 2.0 * math.exp((2.0 * b + s2) * t) * s0 * s0 / (
        (b + s2) * (2.0 * b + s2) * t * t
    ) + 2.0 * s0 * s0 / (b * t * t) * (
        1.0 / (2.0 * b + s2) - math.exp(b * t) / (b + s2)
    )
    vol = math.sqrt(math.log(m2 / (m1 * m1)) / t)
    return black_scholes.black76_price(option_type, m1, strike, m.rate, vol, t)


@dataclasses.dataclass(frozen=True)
class ExchangeAssets:
    """V (received) and U (delivered) with yields, vols and correlation."""

    receive_spot: float
    deliver_spot: float
    receive_yield: float = 0.0
    deliver_yield: float = 0.0
    receive_vol: float = 0.0
    deliver_vol: float = 0.0
    correlation: float = 0.0


def exchange_option_price(a: ExchangeAssets, t: float) -> float:
    """max(V_T - U_T, 0) by Margrabe's formula (Ch 26.14)."""
    var = (
        a.receive_vol**2
        + a.deliver_vol**2
        - 2.0 * a.correlation * a.receive_vol * a.deliver_vol
    )
    vt = math.sqrt(var * t)
    d1 = (
        math.log(a.receive_spot / a.deliver_spot)
        + (a.deliver_yield - a.receive_yield + 0.5 * var) * t
    ) / vt
    d2 = d1 - vt
    return a.receive_spot * math.exp(-a.receive_yield * t) * _n(
        d1
    ) - a.deliver_spot * math.exp(-a.deliver_yield * t) * _n(d2)


def variance_swap_fair_variance(
    m: TreeMarket,
    t: float,
    strikes: Sequence[float],
    implied_vol: Callable[[float], float],
) -> float:
    """Fair variance from out-of-the-money options on a grid (eq. 26.6).

    Raises:
        ValueError: For fewer than 3 increasing positive strikes or F0
            outside the grid.
    """
    n = len(strikes)
    if n < 3 or not t > 0.0 or not strikes[0] > 0.0:
        raise ValueError("need 3 or more positive strikes and t > 0")
    if any(not strikes[i] > strikes[i - 1] for i in range(1, n)):
        raise ValueError("strikes must increase")
    forward = m.spot * math.exp((m.rate - m.yield_) * t)
    if not strikes[0] <= forward <= strikes[-1]:
        raise ValueError("the forward lies outside the strike grid")
    star = 0
    while star + 1 < n and strikes[star + 1] <= forward:
        star += 1
    s_star = strikes[star]
    total = 0.0
    for i, k in enumerate(strikes):
        lo = k if i == 0 else strikes[i - 1]
        hi = k if i + 1 == n else strikes[i + 1]
        dk = (hi - lo) / (1.0 if i in (0, n - 1) else 2.0)
        vol = implied_vol(k)

        def price(
            option_type: OptionType, k: float = k, vol: float = vol
        ) -> float:
            return black_scholes.bsm_price(
                option_type, m.spot, k, m.rate, m.yield_, vol, t
            )

        if i < star:
            q = price(OptionType.PUT)
        elif i > star:
            q = price(OptionType.CALL)
        else:
            q = 0.5 * (price(OptionType.PUT) + price(OptionType.CALL))
        total += dk / (k * k) * math.exp(m.rate * t) * q
    return (
        2.0 / t * math.log(forward / s_star)
        - 2.0 / t * (forward / s_star - 1.0)
        + 2.0 / t * total
    )
