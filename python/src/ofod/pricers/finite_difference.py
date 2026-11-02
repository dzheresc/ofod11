"""Finite difference methods for vanilla options (Hull, Ch 21.8)."""

import dataclasses
import enum
import math

from ofod.instruments import option as option_lib
from ofod.pricers import binomial_tree


class FdScheme(enum.Enum):
    """Time-stepping scheme."""

    EXPLICIT = "explicit"
    IMPLICIT = "implicit"
    CRANK_NICOLSON = "crank_nicolson"


class AmericanSolver(enum.Enum):
    """How the implicit schemes impose early exercise."""

    PSOR = "psor"
    PROJECTION = "projection"


@dataclasses.dataclass(frozen=True)
class FdGrid:
    """Uniform grid in S or, with log_space, in ln S."""

    s_min: float = 0.0
    s_max: float = 0.0
    price_steps: int = 100
    time_steps: int = 100
    log_space: bool = False


def _solve_tridiagonal(
    lo: list[float], mid: list[float], hi: list[float], rhs: list[float]
) -> list[float]:
    mid = list(mid)
    rhs = list(rhs)
    n = len(rhs)
    for i in range(1, n):
        w = lo[i] / mid[i - 1]
        mid[i] -= w * hi[i - 1]
        rhs[i] -= w * rhs[i - 1]
    x = [0.0] * n
    x[-1] = rhs[-1] / mid[-1]
    for i in range(n - 2, -1, -1):
        x[i] = (rhs[i] - hi[i] * x[i + 1]) / mid[i]
    return x


def _psor(
    lo: list[float],
    mid: list[float],
    hi: list[float],
    rhs: list[float],
    floor: list[float],
    x: list[float],
) -> None:
    omega = 1.2
    n = len(rhs)
    for _ in range(20000):
        change = 0.0
        for i in range(n):
            r = rhs[i]
            if i > 0:
                r -= lo[i] * x[i - 1]
            if i + 1 < n:
                r -= hi[i] * x[i + 1]
            nxt = max(floor[i], x[i] + omega * (r / mid[i] - x[i]))
            change = max(change, abs(nxt - x[i]))
            x[i] = nxt
        if change < 1e-12:
            return


def price_finite_difference(
    option: option_lib.VanillaOption,
    market: binomial_tree.TreeMarket,
    grid: FdGrid,
    scheme: FdScheme,
    american: AmericanSolver = AmericanSolver.PSOR,
) -> float:
    """Rolls the option back from expiry on `grid`.

    Raises:
        ValueError: For an invalid grid.
    """
    m, n = grid.price_steps, grid.time_steps
    if (
        m < 3
        or n < 1
        or not grid.s_max > grid.s_min
        or not grid.s_min <= market.spot <= grid.s_max
        or (grid.log_space and grid.s_min <= 0.0)
        or grid.s_min < 0.0
    ):
        raise ValueError("invalid finite difference grid")
    r, q, sigma = market.rate, market.yield_, market.volatility
    dt = option.maturity / n
    is_american = option.exercise is option_lib.Exercise.AMERICAN
    call = option.option_type is option_lib.OptionType.CALL
    x_lo = math.log(grid.s_min) if grid.log_space else grid.s_min
    x_hi = math.log(grid.s_max) if grid.log_space else grid.s_max
    h = (x_hi - x_lo) / m
    xs = [x_lo + h * j for j in range(m + 1)]
    spot = [math.exp(x) for x in xs] if grid.log_space else xs
    stencil = []
    for s in spot:
        if grid.log_space:
            diffusion, drift = 0.5 * sigma * sigma, r - q - 0.5 * sigma * sigma
        else:
            diffusion, drift = 0.5 * sigma * sigma * s * s, (r - q) * s
        stencil.append(
            (
                diffusion / (h * h) - drift / (2.0 * h),
                -2.0 * diffusion / (h * h),
                diffusion / (h * h) + drift / (2.0 * h),
            )
        )
    intrinsic = [option_lib.intrinsic_value(option, s) for s in spot]
    k = option.strike

    def lower(tau: float) -> float:
        if call:
            return 0.0
        if is_american:
            return k - spot[0]
        return max(k * math.exp(-r * tau) - spot[0] * math.exp(-q * tau), 0.0)

    def upper(tau: float) -> float:
        if not call:
            return 0.0
        if is_american:
            return spot[-1] - k
        return max(spot[-1] * math.exp(-q * tau) - k * math.exp(-r * tau), 0.0)

    f = list(intrinsic)
    theta = 0.5 if scheme is FdScheme.CRANK_NICOLSON else 1.0
    for step in range(1, n + 1):
        tau = dt * step
        nxt = [0.0] * (m + 1)
        nxt[0], nxt[-1] = lower(tau), upper(tau)
        if scheme is FdScheme.EXPLICIT:
            for j in range(1, m):
                a, b, c = stencil[j]
                v = (f[j] + dt * (a * f[j - 1] + b * f[j] + c * f[j + 1])) / (
                    1.0 + r * dt
                )
                nxt[j] = max(v, intrinsic[j]) if is_american else v
        else:
            lo, mid, hi, rhs = [], [], [], []
            for j in range(1, m):
                a, b, c = stencil[j]
                lo.append(-theta * dt * a)
                mid.append(1.0 - theta * dt * (b - r))
                hi.append(-theta * dt * c)
                part = a * f[j - 1] + (b - r) * f[j] + c * f[j + 1]
                rhs.append(f[j] + (1.0 - theta) * dt * part)
            rhs[0] -= lo[0] * nxt[0]
            rhs[-1] -= hi[-1] * nxt[-1]
            lo[0] = 0.0
            hi[-1] = 0.0
            if is_american and american is AmericanSolver.PROJECTION:
                x = _solve_tridiagonal(lo, mid, hi, rhs)
                x = [max(v, intrinsic[i + 1]) for i, v in enumerate(x)]
            elif is_american:
                x = f[1:-1]
                _psor(lo, mid, hi, rhs, intrinsic[1:-1], x)
            else:
                x = _solve_tridiagonal(lo, mid, hi, rhs)
            nxt[1:-1] = x
        f = nxt
    x0 = math.log(market.spot) if grid.log_space else market.spot
    pos = (x0 - x_lo) / h
    j = int(min(max(math.floor(pos), 0), m - 1))
    w = pos - j
    return (1.0 - w) * f[j] + w * f[j + 1]
