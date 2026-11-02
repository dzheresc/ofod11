"""The Greek letters (Hull, Ch 19).

Analytic BSM Greeks, a finite-difference bumper that works with any pricer,
delta-gamma-vega neutral hedges, scenario grids and the delta-hedging
experiment. Theta is per year, vega per unit of volatility and rho per unit
of rate.
"""

from collections.abc import Callable
from collections.abc import Sequence
import dataclasses
import math

import numpy as np

from ofod.instruments import option as option_lib
from ofod.math import normal
from ofod.pricers import black_scholes

OptionType = option_lib.OptionType


@dataclasses.dataclass(frozen=True)
class OptionInputs:
    """Market inputs a pricer is bumped against; `yield_` is q."""

    spot: float = 0.0
    rate: float = 0.0
    yield_: float = 0.0
    volatility: float = 0.0
    maturity: float = 0.0


@dataclasses.dataclass(frozen=True)
class Greeks:
    """Delta, gamma, vega, theta (per year) and rho."""

    delta: float
    gamma: float
    vega: float
    theta: float
    rho: float


Pricer = Callable[[OptionInputs], float]


def bsm_greeks(
    option_type: OptionType, strike: float, inputs: OptionInputs
) -> Greeks:
    """Closed-form Greeks of a European option (Table 19.6)."""
    s, t = inputs.spot, inputs.maturity
    sqrt_t = math.sqrt(t)
    d1 = black_scholes.bsm_d1(
        s, strike, inputs.rate, inputs.yield_, inputs.volatility, t
    )
    d2 = d1 - inputs.volatility * sqrt_t
    dq = math.exp(-inputs.yield_ * t)
    dr = math.exp(-inputs.rate * t)
    pdf = normal.norm_pdf(d1)
    gamma = dq * pdf / (s * inputs.volatility * sqrt_t)
    vega = s * dq * pdf * sqrt_t
    decay = -s * pdf * inputs.volatility * dq / (2.0 * sqrt_t)
    cdf = normal.norm_cdf
    if option_type is OptionType.CALL:
        return Greeks(
            delta=dq * cdf(d1),
            gamma=gamma,
            vega=vega,
            theta=decay
            + inputs.yield_ * s * cdf(d1) * dq
            - inputs.rate * strike * dr * cdf(d2),
            rho=strike * t * dr * cdf(d2),
        )
    return Greeks(
        delta=dq * (cdf(d1) - 1.0),
        gamma=gamma,
        vega=vega,
        theta=decay
        - inputs.yield_ * s * cdf(-d1) * dq
        + inputs.rate * strike * dr * cdf(-d2),
        rho=-strike * t * dr * cdf(-d2),
    )


@dataclasses.dataclass(frozen=True)
class BumpSizes:
    """Bump sizes: `spot` is relative to S0, the rest absolute."""

    spot: float = 1e-3
    volatility: float = 1e-4
    rate: float = 1e-4
    time: float = 1.0 / 365.0


def bump_greeks(
    pricer: Pricer, inputs: OptionInputs, bumps: BumpSizes | None = None
) -> Greeks:
    """Central differences of `pricer`; theta is one-sided for short T."""
    bumps = bumps or BumpSizes()
    rep = dataclasses.replace
    v0 = pricer(inputs)
    hs = inputs.spot * bumps.spot
    up = pricer(rep(inputs, spot=inputs.spot + hs))
    down = pricer(rep(inputs, spot=inputs.spot - hs))
    hv = bumps.volatility
    vega = (
        pricer(rep(inputs, volatility=inputs.volatility + hv))
        - pricer(rep(inputs, volatility=inputs.volatility - hv))
    ) / (2.0 * hv)
    hr = bumps.rate
    rho = (
        pricer(rep(inputs, rate=inputs.rate + hr))
        - pricer(rep(inputs, rate=inputs.rate - hr))
    ) / (2.0 * hr)
    ht = bumps.time
    longer = pricer(rep(inputs, maturity=inputs.maturity + ht))
    if inputs.maturity > ht:
        shorter = pricer(rep(inputs, maturity=inputs.maturity - ht))
        theta = (shorter - longer) / (2.0 * ht)
    else:
        theta = (v0 - longer) / ht
    return Greeks(
        delta=(up - down) / (2.0 * hs),
        gamma=(up - 2.0 * v0 + down) / (hs * hs),
        vega=vega,
        theta=theta,
        rho=rho,
    )


@dataclasses.dataclass(frozen=True)
class Exposure:
    """Delta, gamma and vega of a position or of one traded option."""

    delta: float = 0.0
    gamma: float = 0.0
    vega: float = 0.0


@dataclasses.dataclass(frozen=True)
class NeutralHedge:
    """Units of the traded options and of the underlying to hold."""

    option1: float
    option2: float
    underlying: float


def gamma_neutral_hedge(portfolio: Exposure, option: Exposure) -> NeutralHedge:
    """Gamma neutral with one option, then delta neutral (Ch 19.6).

    Raises:
        ValueError: If the option has no gamma.
    """
    if option.gamma == 0.0:
        raise ValueError("the hedging option has zero gamma")
    w = -portfolio.gamma / option.gamma
    return NeutralHedge(w, 0.0, -(portfolio.delta + w * option.delta))


def gamma_vega_neutral_hedge(
    portfolio: Exposure, option1: Exposure, option2: Exposure
) -> NeutralHedge:
    """Gamma and vega neutral with two options, then delta neutral.

    Raises:
        ValueError: If the options' (gamma, vega) vectors are parallel.
    """
    det = option1.gamma * option2.vega - option2.gamma * option1.vega
    if abs(det) < 1e-14:
        raise ValueError("the hedging options are linearly dependent")
    w1 = (
        -portfolio.gamma * option2.vega + portfolio.vega * option2.gamma
    ) / det
    w2 = (
        -portfolio.vega * option1.gamma + portfolio.gamma * option1.vega
    ) / det
    return NeutralHedge(
        w1,
        w2,
        -(portfolio.delta + w1 * option1.delta + w2 * option2.delta),
    )


def scenario_grid(
    pricer: Pricer,
    base: OptionInputs,
    spots: Sequence[float],
    volatilities: Sequence[float],
) -> list[list[float]]:
    """Change in value for each (volatility, spot) pair (Table 19.8)."""
    v0 = pricer(base)
    return [
        [
            pricer(dataclasses.replace(base, spot=s, volatility=v)) - v0
            for s in spots
        ]
        for v in volatilities
    ]


@dataclasses.dataclass(frozen=True)
class HedgeExperiment:
    """Inputs of the delta-hedging experiment; `drift` is real-world mu."""

    spot: float = 0.0
    strike: float = 0.0
    rate: float = 0.0
    drift: float = 0.0
    volatility: float = 0.0
    maturity: float = 0.0


@dataclasses.dataclass(frozen=True)
class HedgePerformance:
    """Discounted hedging cost statistics; `ratio` is std / BSM price."""

    mean_cost: float
    std_cost: float
    bsm_price: float
    ratio: float


def simulate_delta_hedge(
    experiment: HedgeExperiment, rebalances: int, paths: int, seed: int
) -> HedgePerformance:
    """Writes one call and delta hedges it (Ch 19.4).

    The C++ version draws from mt19937_64, this one from NumPy, so the two
    agree only statistically.

    Raises:
        ValueError: If rebalances < 1 or paths < 2.
    """
    if rebalances < 1 or paths < 2:
        raise ValueError("need rebalances >= 1 and paths >= 2")
    e = experiment
    dt = e.maturity / rebalances
    growth = math.exp(e.rate * dt)
    drift = (e.drift - 0.5 * e.volatility**2) * dt
    diffusion = e.volatility * math.sqrt(dt)
    rng = np.random.default_rng(seed)

    def delta_at(spot: np.ndarray, tau: float) -> np.ndarray:
        d1 = (
            np.log(spot / e.strike) + (e.rate + 0.5 * e.volatility**2) * tau
        ) / (e.volatility * math.sqrt(tau))
        return np.asarray(0.5 * _erfc(-d1 / math.sqrt(2.0)))

    spot = np.full(paths, e.spot)
    shares = delta_at(spot, e.maturity)
    cash = -shares * spot
    for i in range(1, rebalances):
        spot = spot * np.exp(drift + diffusion * rng.standard_normal(paths))
        cash = cash * growth
        target = delta_at(spot, e.maturity - i * dt)
        cash = cash - (target - shares) * spot
        shares = target
    spot = spot * np.exp(drift + diffusion * rng.standard_normal(paths))
    cash = cash * growth + shares * spot - np.maximum(spot - e.strike, 0.0)
    cost = -cash * math.exp(-e.rate * e.maturity)
    price = black_scholes.bsm_price(
        OptionType.CALL, e.spot, e.strike, e.rate, 0.0, e.volatility, e.maturity
    )
    std = float(cost.std(ddof=1))
    return HedgePerformance(float(cost.mean()), std, price, std / price)


_erfc = np.vectorize(math.erfc, otypes=[float])
