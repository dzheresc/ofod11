"""Recombining binomial trees for vanilla options (Hull, Ch 13, 21.1).

Options on an index, a currency or a futures contract use the same tree
with q = dividend yield, foreign rate or r respectively.
"""

from collections.abc import Sequence
import dataclasses
import enum
import math

from ofod.instruments import cash_flow
from ofod.instruments import option as option_lib
from ofod.pricers import black_scholes


@dataclasses.dataclass(frozen=True)
class TreeStep:
    """Up and down factors and the risk-neutral up probability."""

    up: float
    down: float
    probability: float


def risk_neutral_probability(up: float, down: float, growth: float) -> float:
    """Returns p = (a - d) / (u - d) (eq. 13.3)."""
    return (growth - down) / (up - down)


class TreeScheme(enum.Enum):
    """Parameterisations of u, d and p."""

    COX_ROSS_RUBINSTEIN = "crr"
    JARROW_RUDD = "jr"
    TIAN = "tian"


def scheme_step(
    scheme: TreeScheme, volatility: float, drift: float, dt: float
) -> TreeStep:
    """Returns the step parameters of `scheme`; drift is r - q."""
    if scheme is TreeScheme.COX_ROSS_RUBINSTEIN:
        up = math.exp(volatility * math.sqrt(dt))
        down = 1.0 / up
        return TreeStep(
            up, down, risk_neutral_probability(up, down, math.exp(drift * dt))
        )
    if scheme is TreeScheme.JARROW_RUDD:
        mu = (drift - 0.5 * volatility * volatility) * dt
        jump = volatility * math.sqrt(dt)
        return TreeStep(math.exp(mu + jump), math.exp(mu - jump), 0.5)
    a = math.exp(drift * dt)
    v = math.exp(volatility * volatility * dt)
    root = math.sqrt(v * v + 2.0 * v - 3.0)
    up = 0.5 * a * v * (v + 1.0 + root)
    down = 0.5 * a * v * (v + 1.0 - root)
    return TreeStep(up, down, risk_neutral_probability(up, down, a))


@dataclasses.dataclass(frozen=True)
class TreeMarket:
    """Market inputs; `yield_` is q (dividend yield, foreign rate or r)."""

    spot: float = 0.0
    rate: float = 0.0
    yield_: float = 0.0
    volatility: float = 0.0


@dataclasses.dataclass(frozen=True)
class TreeResult:
    """Price and the first-step delta (eq. 13.8)."""

    price: float
    delta: float


def price_tree(
    option: option_lib.VanillaOption,
    spot: float,
    rate: float,
    steps: int,
    step: TreeStep,
) -> TreeResult:
    """Prices `option` by backward induction.

    Raises:
        ValueError: If steps < 1, u <= d or p lies outside [0, 1].
    """
    u, d, p = step.up, step.down, step.probability
    if steps < 1 or not u > d or not 0.0 <= p <= 1.0:
        raise ValueError("invalid tree: need steps >= 1, u > d, 0 <= p <= 1")
    dt = option.maturity / steps
    disc = math.exp(-rate * dt)
    american = option.exercise is option_lib.Exercise.AMERICAN
    intrinsic = option_lib.intrinsic_value

    def node(i: int, j: int) -> float:
        return spot * math.pow(u, j) * math.pow(d, i - j)

    values = [intrinsic(option, node(steps, j)) for j in range(steps + 1)]
    value_up = value_down = 0.0
    for i in range(steps - 1, -1, -1):
        for j in range(i + 1):
            v = disc * (p * values[j + 1] + (1.0 - p) * values[j])
            if american:
                v = max(v, intrinsic(option, node(i, j)))
            values[j] = v
        if i == 1:
            value_down, value_up = values[0], values[1]
    if steps == 1:
        value_down = intrinsic(option, spot * d)
        value_up = intrinsic(option, spot * u)
    delta = (value_up - value_down) / (spot * u - spot * d)
    return TreeResult(values[0], delta)


def price_binomial(
    option: option_lib.VanillaOption,
    market: TreeMarket,
    steps: int,
    scheme: TreeScheme = TreeScheme.COX_ROSS_RUBINSTEIN,
) -> TreeResult:
    """Prices `option` on a `steps`-step tree built with `scheme`.

    Raises:
        ValueError: If steps < 1 or the tree admits arbitrage.
    """
    if steps < 1:
        raise ValueError("steps must be >= 1")
    dt = option.maturity / steps
    step = scheme_step(
        scheme, market.volatility, market.rate - market.yield_, dt
    )
    return price_tree(option, market.spot, market.rate, steps, step)


def price_binomial_with_dividends(
    option: option_lib.VanillaOption,
    market: TreeMarket,
    steps: int,
    dividends: Sequence[cash_flow.CashFlow],
) -> TreeResult:
    """CRR tree on S* = S - PV(dividends), dividends added back (Ch 21.3).

    Raises:
        ValueError: If steps < 1 or market.yield_ is not zero.
    """
    if steps < 1 or market.yield_ != 0.0:
        raise ValueError("need steps >= 1 and a zero yield")
    t = option.maturity
    dt = t / steps

    def remaining_pv(now: float) -> float:
        return sum(
            d.amount * math.exp(-market.rate * (d.time - now))
            for d in dividends
            if now < d.time <= t
        )

    s_star = market.spot - remaining_pv(0.0)
    step = scheme_step(
        TreeScheme.COX_ROSS_RUBINSTEIN, market.volatility, market.rate, dt
    )
    u, d, p = step.up, step.down, step.probability
    disc = math.exp(-market.rate * dt)
    american = option.exercise is option_lib.Exercise.AMERICAN
    intrinsic = option_lib.intrinsic_value
    pv_at = [remaining_pv(i * dt) for i in range(steps + 1)]

    def node(i: int, j: int) -> float:
        return s_star * math.pow(u, j) * math.pow(d, i - j) + pv_at[i]

    values = [intrinsic(option, node(steps, j)) for j in range(steps + 1)]
    value_up = value_down = 0.0
    for i in range(steps - 1, -1, -1):
        for j in range(i + 1):
            v = disc * (p * values[j + 1] + (1.0 - p) * values[j])
            if american:
                v = max(v, intrinsic(option, node(i, j)))
            values[j] = v
        if i == 1:
            value_down, value_up = values[0], values[1]
    if steps == 1:
        value_down = intrinsic(option, node(1, 0))
        value_up = intrinsic(option, node(1, 1))
    delta = (value_up - value_down) / (node(1, 1) - node(1, 0))
    return TreeResult(values[0], delta)


def control_variate_american_price(
    option: option_lib.VanillaOption, market: TreeMarket, steps: int
) -> float:
    """American tree + BSM European - European tree (Ch 21.3)."""
    american = dataclasses.replace(
        option, exercise=option_lib.Exercise.AMERICAN
    )
    european = dataclasses.replace(
        option, exercise=option_lib.Exercise.EUROPEAN
    )
    a = price_binomial(american, market, steps).price
    e = price_binomial(european, market, steps).price
    bsm = black_scholes.bsm_price(
        option.option_type,
        market.spot,
        option.strike,
        market.rate,
        market.yield_,
        market.volatility,
        option.maturity,
    )
    return a + bsm - e


def price_trinomial(
    option: option_lib.VanillaOption, market: TreeMarket, steps: int
) -> TreeResult:
    """Trinomial tree (Ch 21.4).

    Raises:
        ValueError: If steps < 1, sigma <= 0 or a probability is negative.
    """
    if steps < 1 or not market.volatility > 0.0:
        raise ValueError("need steps >= 1 and a positive volatility")
    dt = option.maturity / steps
    sigma = market.volatility
    u = math.exp(sigma * math.sqrt(3.0 * dt))
    nu = market.rate - market.yield_ - 0.5 * sigma * sigma
    tilt = nu * math.sqrt(dt / (12.0 * sigma * sigma))
    pu = 1.0 / 6.0 + tilt
    pd = 1.0 / 6.0 - tilt
    pm = 2.0 / 3.0
    if pu < 0.0 or pd < 0.0:
        raise ValueError("negative trinomial probability")
    disc = math.exp(-market.rate * dt)
    american = option.exercise is option_lib.Exercise.AMERICAN
    intrinsic = option_lib.intrinsic_value
    n = steps
    level = [market.spot * math.pow(u, k) for k in range(-n, n + 1)]
    values = [intrinsic(option, s) for s in level]
    value_up = value_down = 0.0
    for i in range(n - 1, -1, -1):
        for k in range(-i, i + 1):
            base = k + i
            v = disc * (
                pd * values[base]
                + pm * values[base + 1]
                + pu * values[base + 2]
            )
            if american:
                v = max(v, intrinsic(option, level[k + n]))
            values[base] = v
        if i == 1:
            value_down, value_up = values[0], values[2]
    if n == 1:
        value_down = intrinsic(option, level[n - 1])
        value_up = intrinsic(option, level[n + 1])
    delta = (value_up - value_down) / (level[n + 1] - level[n - 1])
    return TreeResult(values[0], delta)
