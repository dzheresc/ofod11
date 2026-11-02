"""Longstaff-Schwartz least-squares Monte Carlo and convertible bonds.

Hull, Ch 27.8 and 27.4. The NumPy stream differs from the C++ library's,
so LSM prices agree only within Monte Carlo error.
"""

import dataclasses
import math

import numpy as np

from ofod.instruments.option import OptionType
from ofod.math import linalg
from ofod.pricers import binomial_tree
from ofod.pricers import monte_carlo

TreeMarket = binomial_tree.TreeMarket


@dataclasses.dataclass(frozen=True)
class LsmOptions:
    """Exercise dates, paths, seed and polynomial degree in S/K."""

    exercise_dates: int = 50
    paths: int = 100_000
    seed: int = 1
    basis_degree: int = 2


def _intrinsic(option_type: OptionType, s: np.ndarray, k: float) -> np.ndarray:
    payoff = s - k if option_type is OptionType.CALL else k - s
    return np.maximum(payoff, 0.0)


def longstaff_schwartz(
    option_type: OptionType,
    m: TreeMarket,
    strike: float,
    t: float,
    options: LsmOptions,
) -> monte_carlo.McResult:
    """American option by regression on in-the-money paths.

    Raises:
        ValueError: For dates < 1, paths < 2 or degree outside [1, 6].
    """
    dates, deg, n = options.exercise_dates, options.basis_degree, options.paths
    if dates < 1 or n < 2 or not 1 <= deg <= 6 or not t > 0.0 or not strike > 0:
        raise ValueError("invalid Longstaff-Schwartz settings")
    dt = t / dates
    drift = (m.rate - m.yield_ - 0.5 * m.volatility**2) * dt
    diffusion = m.volatility * math.sqrt(dt)
    df = math.exp(-m.rate * dt)
    rng = np.random.default_rng(options.seed)
    z = rng.standard_normal((dates, n))
    spot = m.spot * np.exp(np.cumsum(drift + diffusion * z, axis=0))
    cash = _intrinsic(option_type, spot[-1], strike)
    for j in range(dates - 2, -1, -1):
        cash *= df
        exercise = _intrinsic(option_type, spot[j], strike)
        itm = exercise > 0.0
        if itm.sum() <= deg:
            continue
        x = spot[j, itm] / strike
        basis = np.vander(x, deg + 1, increasing=True)
        normal = (basis.T @ basis).tolist()
        rhs = (basis.T @ cash[itm]).tolist()
        try:
            beta = np.asarray(linalg.solve_linear(normal, rhs))
        except ValueError:
            continue
        continuation = basis @ beta
        idx = np.flatnonzero(itm)
        better = exercise[itm] > continuation
        cash[idx[better]] = exercise[itm][better]
    cash *= df
    intrinsic_now = max(
        m.spot - strike if option_type is OptionType.CALL else strike - m.spot,
        0.0,
    )
    return monte_carlo.McResult(
        max(float(cash.mean()), intrinsic_now),
        float(cash.std(ddof=1)) / math.sqrt(n),
        n,
    )


@dataclasses.dataclass(frozen=True)
class ConvertibleTerms:
    """A zero-coupon convertible with an issuer call and default risk."""

    face: float = 100.0
    conversion_ratio: float = 1.0
    maturity: float = 1.0
    call_price: float = math.inf
    recovery: float = 0.4
    hazard: float = 0.0


def convertible_bond_price(
    m: TreeMarket, terms: ConvertibleTerms, steps: int
) -> float:
    """Hull's tree with default (Ch 27.4).

    Raises:
        ValueError: Unless steps >= 1, sigma^2 > lambda and the
            probabilities are valid.
    """
    if steps < 1 or not terms.maturity > 0.0:
        raise ValueError("need steps >= 1 and a positive maturity")
    dt = terms.maturity / steps
    var = (m.volatility**2 - terms.hazard) * dt
    if not var > 0.0:
        raise ValueError("need sigma^2 > hazard")
    u = math.exp(math.sqrt(var))
    d = 1.0 / u
    a = math.exp((m.rate - m.yield_) * dt)
    survive = math.exp(-terms.hazard * dt)
    pu = (a - d * survive) / (u - d)
    pd = (u * survive - a) / (u - d)
    if pu < 0.0 or pd < 0.0:
        raise ValueError("negative tree probability")
    p_default = 1.0 - survive
    disc = math.exp(-m.rate * dt)
    recovered = terms.recovery * terms.face
    up = [u**j for j in range(steps + 1)]

    def spot_at(i: int, j: int) -> float:
        return m.spot * up[j] / up[i - j]

    value = [
        max(terms.face, terms.conversion_ratio * spot_at(steps, j))
        for j in range(steps + 1)
    ]
    for i in range(steps - 1, -1, -1):
        for j in range(i + 1):
            hold = disc * (
                pu * value[j + 1] + pd * value[j] + p_default * recovered
            )
            conversion = terms.conversion_ratio * spot_at(i, j)
            value[j] = max(conversion, min(hold, terms.call_price))
    return value[0]
