"""Monte Carlo valuation under geometric Brownian motion (Ch 21.6-21.7).

Same algorithms as the C++ library; the random streams differ (NumPy here),
so results agree statistically rather than exactly.
"""

from collections.abc import Callable
from collections.abc import Sequence
import dataclasses
import enum
import math

import numpy as np
from numpy import typing as npt

from ofod.instruments import option as option_lib
from ofod.math import normal
from ofod.pricers import binomial_tree
from ofod.random import sobol

CHUNK_PATHS = 1024
_BATCHES = 16
_MASK = (1 << 64) - 1


class VarianceReduction(enum.Enum):
    """Variance reduction techniques of Ch 21.7."""

    NONE = "none"
    ANTITHETIC = "antithetic"
    CONTROL_VARIATE = "control_variate"
    IMPORTANCE_SAMPLING = "importance_sampling"
    STRATIFIED = "stratified"
    MOMENT_MATCHING = "moment_matching"
    QUASI_RANDOM = "quasi_random"


class Rng(enum.Enum):
    """Normal generator of the C++ library's MonteCarloEuropean.

    Accepted for API parity: NumPy's default generator (PCG64 with a
    ziggurat for normals) is used either way.
    """

    MERSENNE_TWISTER = "mersenne_twister"
    ZIGGURAT = "ziggurat"


@dataclasses.dataclass(frozen=True)
class McOptions:
    """Simulation settings; `threads` and `rng` are accepted for API parity."""

    paths: int = 100_000
    seed: int = 1
    method: VarianceReduction = VarianceReduction.NONE
    threads: int = 1
    rng: Rng = Rng.MERSENNE_TWISTER


@dataclasses.dataclass(frozen=True)
class McResult:
    """Estimate, its standard error and the number of paths used."""

    price: float
    std_error: float
    paths: int


def mix_seed(seed: int, chunk: int) -> int:
    """SplitMix64 of (seed, chunk), as in the C++ library."""
    z = (seed + 0x9E3779B97F4A7C15 * (chunk + 1)) & _MASK
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & _MASK
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & _MASK
    return z ^ (z >> 31)


def _batch_normals(
    method: VarianceReduction, n: int, seed: int
) -> npt.NDArray[np.float64]:
    rng = np.random.default_rng(seed)
    if method is VarianceReduction.STRATIFIED:
        u = (np.arange(n) + rng.random(n)) / n
    elif method is VarianceReduction.QUASI_RANDOM:
        seq = sobol.Sobol(1)
        shift = rng.random()
        u = np.array([seq.next()[0] for _ in range(n)]) + shift
        u = np.where(u >= 1.0, u - 1.0, u)
    else:
        z = rng.standard_normal(n)
        return (z - z.mean()) / z.std(ddof=1)
    u = np.clip(u, 1e-16, 1.0 - 1e-16)
    return np.array([normal.norm_inv(float(x)) for x in u])


def monte_carlo_european(
    option: option_lib.VanillaOption,
    market: binomial_tree.TreeMarket,
    options: McOptions,
) -> McResult:
    """Prices a European option by sampling S_T directly.

    Raises:
        ValueError: For American options or fewer than 32 paths.
    """
    if (
        option.exercise is not option_lib.Exercise.EUROPEAN
        or options.paths < 32
    ):
        raise ValueError("need a European option and at least 32 paths")
    t = option.maturity
    sigma = market.volatility
    mu = (market.rate - market.yield_ - 0.5 * sigma * sigma) * t
    vol = sigma * math.sqrt(t)
    disc = math.exp(-market.rate * t)
    k = option.strike
    call = option.option_type is option_lib.OptionType.CALL

    def payoff(s: npt.NDArray[np.float64]) -> npt.NDArray[np.float64]:
        return np.maximum(s - k, 0.0) if call else np.maximum(k - s, 0.0)

    def terminal(z: npt.NDArray[np.float64]) -> npt.NDArray[np.float64]:
        return market.spot * np.exp(mu + vol * z)

    n = options.paths
    m = options.method
    if m in (
        VarianceReduction.STRATIFIED,
        VarianceReduction.MOMENT_MATCHING,
        VarianceReduction.QUASI_RANDOM,
    ):
        per_batch = n // _BATCHES
        means = np.array(
            [
                payoff(
                    terminal(
                        _batch_normals(m, per_batch, mix_seed(options.seed, b))
                    )
                ).mean()
                for b in range(_BATCHES)
            ]
        )
        return McResult(
            disc * float(means.mean()),
            disc * float(means.std(ddof=1)) / math.sqrt(_BATCHES),
            per_batch * _BATCHES,
        )
    z = np.random.default_rng(options.seed).standard_normal(n)
    if m is VarianceReduction.ANTITHETIC:
        f = 0.5 * (payoff(terminal(z)) + payoff(terminal(-z)))
    elif m is VarianceReduction.IMPORTANCE_SAMPLING:
        to_strike = (math.log(k / market.spot) - mu) / vol
        theta = max(to_strike, 0.0) if call else min(to_strike, 0.0)
        y = z + theta
        f = payoff(terminal(y)) * np.exp(-theta * y + 0.5 * theta * theta)
    else:
        s = terminal(z)
        f = payoff(s)
        if m is VarianceReduction.CONTROL_VARIATE:
            expected = market.spot * math.exp((market.rate - market.yield_) * t)
            b = np.cov(f, s, ddof=1)[0, 1] / s.var(ddof=1)
            f = f - b * (s - expected)
    return McResult(
        disc * float(f.mean()), disc * float(f.std(ddof=1)) / math.sqrt(n), n
    )


def simulate_gbm_paths(
    market: binomial_tree.TreeMarket,
    t: float,
    steps: int,
    payoff: Callable[[Sequence[float]], float],
    options: McOptions,
) -> McResult:
    """Discounted mean of `payoff(path)` over risk-neutral GBM paths.

    `path` holds S at 0, dt, ..., t. Paths come in chunks of CHUNK_PATHS
    seeded with mix_seed(seed, chunk). Supports NONE and ANTITHETIC.

    Raises:
        ValueError: For invalid settings or another variance reduction.
    """
    if steps < 1 or options.paths < 2 or options.threads < 1:
        raise ValueError("need steps >= 1, paths >= 2 and threads >= 1")
    if options.method not in (
        VarianceReduction.NONE,
        VarianceReduction.ANTITHETIC,
    ):
        raise ValueError("path simulator supports NONE and ANTITHETIC only")
    antithetic = options.method is VarianceReduction.ANTITHETIC
    dt = t / steps
    mu = (market.rate - market.yield_ - 0.5 * market.volatility**2) * dt
    jump = market.volatility * math.sqrt(dt)
    values = []
    for c in range((options.paths + CHUNK_PATHS - 1) // CHUNK_PATHS):
        rng = np.random.default_rng(mix_seed(options.seed, c))
        count = min(CHUNK_PATHS, options.paths - c * CHUNK_PATHS)
        z = rng.standard_normal((count, steps))
        for sign in (1.0, -1.0) if antithetic else (1.0,):
            log_paths = np.cumsum(mu + jump * sign * z, axis=1)
            paths = market.spot * np.exp(
                np.hstack([np.zeros((count, 1)), log_paths])
            )
            values.append([payoff(list(row)) for row in paths])
    if antithetic:
        v = 0.5 * (np.concatenate(values[0::2]) + np.concatenate(values[1::2]))
    else:
        v = np.concatenate(values)
    disc = math.exp(-market.rate * t)
    return McResult(
        disc * float(v.mean()),
        disc * float(v.std(ddof=1)) / math.sqrt(len(v)),
        len(v),
    )
