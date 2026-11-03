"""Commodity price models (Hull, Ch 35.3-35.4).

A seasonal forward curve fitted to futures prices and a mean-reverting
log-price with jumps for spiky markets such as electricity.
"""

from collections.abc import Sequence
import dataclasses
import math

import numpy as np

from ofod.math import linalg
from ofod.math import quadrature

_MONTHS = 12


def _month_of(t: float) -> int:
    return math.floor(12.0 * t + 1e-9) % _MONTHS


@dataclasses.dataclass(frozen=True)
class SeasonalCurve:
    """ln F(t) = level + growth t + factors[month(t)]; factors sum to 0."""

    level: float = 0.0
    growth: float = 0.0
    factors: tuple[float, ...] = (0.0,) * _MONTHS

    def forward(self, t: float) -> float:
        """Returns the forward price for maturity t."""
        return math.exp(
            self.level + self.growth * t + self.factors[_month_of(t)]
        )

    def multiplier(self, t: float) -> float:
        """Returns the seasonal multiplier of the month containing t."""
        return math.exp(self.factors[_month_of(t)])


def fit_seasonal_curve(
    maturities: Sequence[float], futures: Sequence[float]
) -> SeasonalCurve:
    """Least-squares fit of ln F on time and month dummies.

    Raises:
        ValueError: For fewer than 14 futures, a missing calendar month or a
            non-positive price.
    """
    n = len(maturities)
    if n != len(futures) or n < _MONTHS + 2:
        raise ValueError("need at least 14 matching futures")
    if any(not f > 0.0 for f in futures):
        raise ValueError("futures prices must be positive")
    params = _MONTHS + 1
    normal = [[0.0] * params for _ in range(params)]
    rhs = [0.0] * params
    seen = [False] * _MONTHS
    for t, f in zip(maturities, futures, strict=True):
        month = _month_of(t)
        seen[month] = True
        row = [0.0] * params
        row[0] = 1.0
        row[1] = t
        for m in range(_MONTHS - 1):
            if month == m:
                row[2 + m] = 1.0
            if month == _MONTHS - 1:
                row[2 + m] = -1.0
        y = math.log(f)
        for r in range(params):
            rhs[r] += row[r] * y
            for c in range(params):
                normal[r][c] += row[r] * row[c]
    if not all(seen):
        raise ValueError("every calendar month needs a futures price")
    beta = linalg.solve_linear(normal, rhs)
    factors = list(beta[2:])
    factors.append(-sum(factors))
    return SeasonalCurve(beta[0], beta[1], tuple(factors))


@dataclasses.dataclass(frozen=True)
class SpikeModel:
    """ln S = log_level + x, dx = -a x dt + sigma dz + J dN (Ch 35.4)."""

    log_level: float = 0.0
    a: float = 0.0
    sigma: float = 0.0
    intensity: float = 0.0
    jump_mean: float = 0.0
    jump_vol: float = 0.0


@dataclasses.dataclass(frozen=True)
class SpikeEstimate:
    """Monte Carlo mean and its standard error."""

    mean: float
    std_error: float


def _check_spike(m: SpikeModel) -> None:
    if not (
        m.a > 0.0 and m.sigma >= 0.0 and m.intensity >= 0.0 and m.jump_vol >= 0
    ):
        raise ValueError("invalid spike model")


def spike_expected_price(model: SpikeModel, x0: float, t: float) -> float:
    """E[S(T)] given x(0) = x0, in closed form plus a jump integral.

    Raises:
        ValueError: If a <= 0 or T < 0.
    """
    _check_spike(model)
    if t < 0.0:
        raise ValueError("t must be non-negative")
    a = model.a
    mean = x0 * math.exp(-a * t)
    variance = model.sigma**2 * (1.0 - math.exp(-2.0 * a * t)) / (2.0 * a)
    jump_part = 0.0
    if model.intensity > 0.0 and t > 0.0:
        rule = quadrature.gauss_legendre(64, 0.0, t)
        for u, w in zip(rule.nodes, rule.weights, strict=True):
            c = math.exp(-a * u)
            mgf = math.exp(
                model.jump_mean * c + 0.5 * model.jump_vol**2 * c * c
            )
            jump_part += w * (mgf - 1.0)
        jump_part *= model.intensity
    return math.exp(model.log_level + mean + 0.5 * variance + jump_part)


def _step(
    model: SpikeModel, x: np.ndarray, dt: float, rng: np.random.Generator
) -> np.ndarray:
    decay = math.exp(-model.a * dt)
    noise = model.sigma * math.sqrt(
        (1.0 - math.exp(-2.0 * model.a * dt)) / (2.0 * model.a)
    )
    nxt = x * decay + noise * rng.standard_normal(x.shape)
    if model.intensity > 0.0:
        counts = rng.poisson(model.intensity * dt, x.shape)
        total = int(counts.sum())
        if total:
            sizes = model.jump_mean + model.jump_vol * rng.standard_normal(
                total
            )
            ages = rng.random(total)
            contrib = sizes * np.exp(-model.a * dt * (1.0 - ages))
            owner = np.repeat(np.arange(x.size), counts.ravel())
            nxt = nxt + np.bincount(owner, contrib, x.size).reshape(x.shape)
    return np.asarray(nxt)


def simulate_spike_path(
    model: SpikeModel, x0: float, dt: float, steps: int, seed: int
) -> list[float]:
    """One path of S on `steps` steps; element 0 is S(0).

    The C++ version draws from mt19937_64, this one from NumPy.

    Raises:
        ValueError: For bad inputs.
    """
    _check_spike(model)
    if not dt > 0.0 or steps <= 0:
        raise ValueError("need dt > 0 and steps > 0")
    rng = np.random.default_rng(seed)
    x = np.array([x0])
    path = [math.exp(model.log_level + x0)]
    for _ in range(steps):
        x = _step(model, x, dt, rng)
        path.append(math.exp(model.log_level + float(x[0])))
    return path


def spike_monte_carlo(
    model: SpikeModel, x0: float, t: float, steps: int, paths: int, seed: int
) -> SpikeEstimate:
    """Monte Carlo estimate of E[S(T)] with the exact OU-plus-jumps step.

    Raises:
        ValueError: For bad inputs.
    """
    _check_spike(model)
    if not t > 0.0 or steps <= 0 or paths < 2:
        raise ValueError("need t > 0, steps > 0 and paths >= 2")
    rng = np.random.default_rng(seed)
    x = np.full(paths, float(x0))
    for _ in range(steps):
        x = _step(model, x, t / steps, rng)
    s = np.exp(model.log_level + x)
    return SpikeEstimate(
        float(s.mean()), float(s.std(ddof=1) / math.sqrt(paths))
    )
