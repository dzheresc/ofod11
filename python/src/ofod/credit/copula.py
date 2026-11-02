"""One-factor Gaussian copula: CDO tranches and nth-to-default baskets.

Hull, Ch 25.9-25.11. A homogeneous pool's conditional default count is
computed with the same recursion as `default_count_distribution`,
vectorised over the quadrature nodes.
"""

from collections.abc import Sequence
import dataclasses
import math

import numpy as np
from scipy import special

from ofod.credit import tranche as tranche_lib
from ofod.curves import zero_curve as zc
from ofod.math import normal
from ofod.math import quadrature
from ofod.math import roots


def default_count_distribution(p: Sequence[float]) -> list[float]:
    """P(k defaults), k = 0..n, for independent names (ASB recursion)."""
    dist = [1.0]
    for q in p:
        dist.append(0.0)
        for k in range(len(dist) - 1, 0, -1):
            dist[k] = dist[k] * (1.0 - q) + dist[k - 1] * q
        dist[0] *= 1.0 - q
    return dist


@dataclasses.dataclass(frozen=True)
class CdoPool:
    """Equal names with a flat hazard and common recovery."""

    names: int = 125
    hazard: float = 0.01
    recovery: float = 0.4


@dataclasses.dataclass(frozen=True)
class CdoTerms:
    """Maturity, payments a year and Gauss-Hermite points."""

    maturity: float = 5.0
    frequency: int = 4
    quadrature_points: int = 64


@dataclasses.dataclass(frozen=True)
class TrancheLegs:
    """Premium annuity and protection PV, as fractions of tranche width."""

    premium_annuity: float
    protection: float


def _check(tranche: tranche_lib.Tranche, pool: CdoPool, rho: float) -> None:
    ok_pool = (
        pool.names >= 1 and pool.hazard >= 0.0 and 0.0 <= pool.recovery < 1.0
    )
    ok_tranche = 0.0 <= tranche.attachment < tranche.detachment <= 1.0
    if not (ok_pool and ok_tranche and 0.0 <= rho < 1.0):
        raise ValueError("invalid pool, tranche or correlation")


def expected_tranche_loss(
    tranche: tranche_lib.Tranche,
    pool: CdoPool,
    correlation: float,
    t: float,
    quadrature_points: int = 64,
) -> float:
    """Expected loss by time t as a fraction of tranche width.

    Raises:
        ValueError: For an invalid pool, tranche, correlation or rule size.
    """
    _check(tranche, pool, correlation)
    rule = quadrature.gauss_hermite_normal(quadrature_points)
    if t <= 0.0:
        return 0.0
    q = -math.expm1(-pool.hazard * t)
    nodes = np.asarray(rule.nodes)
    weights = np.asarray(rule.weights)
    if q <= 0.0:
        p = np.zeros_like(nodes)
    else:
        p = special.ndtr(
            (normal.norm_inv(q) - math.sqrt(correlation) * nodes)
            / math.sqrt(1.0 - correlation)
        )
    pmf = np.zeros((p.size, pool.names + 1))
    pmf[:, 0] = 1.0
    for n in range(1, pool.names + 1):
        pmf[:, 1 : n + 1] = (
            pmf[:, 1 : n + 1] * (1.0 - p[:, None]) + pmf[:, :n] * p[:, None]
        )
        pmf[:, 0] *= 1.0 - p
    k = np.arange(pool.names + 1)
    pool_loss = (1.0 - pool.recovery) * k / pool.names
    width = tranche.detachment - tranche.attachment
    loss = np.clip((pool_loss - tranche.attachment) / width, 0.0, 1.0)
    return float(weights @ (pmf @ loss))


def tranche_leg_values(
    tranche: tranche_lib.Tranche,
    pool: CdoPool,
    correlation: float,
    terms: CdoTerms,
    discount: zc.ZeroCurve,
) -> TrancheLegs:
    """Premium and protection legs of a tranche.

    Raises:
        ValueError: For invalid inputs.
    """
    if not terms.maturity > 0.0 or terms.frequency < 1:
        raise ValueError("need maturity > 0 and frequency >= 1")
    dt = 1.0 / terms.frequency
    periods = math.ceil(terms.maturity * terms.frequency - 1e-9)
    annuity = protection = start = previous = 0.0
    for i in range(1, periods + 1):
        end = min(i * dt, terms.maturity)
        loss = expected_tranche_loss(
            tranche, pool, correlation, end, terms.quadrature_points
        )
        length = end - start
        df_mid = discount.discount_factor(0.5 * (start + end))
        annuity += (
            length * (1.0 - loss) * discount.discount_factor(end)
            + 0.5 * length * (loss - previous) * df_mid
        )
        protection += (loss - previous) * df_mid
        previous = loss
        start = end
    return TrancheLegs(annuity, protection)


def tranche_spread(
    tranche: tranche_lib.Tranche,
    pool: CdoPool,
    correlation: float,
    terms: CdoTerms,
    discount: zc.ZeroCurve,
) -> float:
    """Breakeven spread on the outstanding tranche notional."""
    legs = tranche_leg_values(tranche, pool, correlation, terms, discount)
    return legs.protection / legs.premium_annuity


def implied_tranche_correlation(
    tranche: tranche_lib.Tranche,
    pool: CdoPool,
    spread: float,
    terms: CdoTerms,
    discount: zc.ZeroCurve,
) -> float:
    """Lowest correlation on [1e-4, 0.999] reproducing `spread`.

    Scans 40 sub-intervals for a sign change, then polishes with Brent.

    Raises:
        ValueError: If no correlation reproduces the spread.
    """

    def gap(rho: float) -> float:
        return tranche_spread(tranche, pool, rho, terms, discount) - spread

    grid, lo_end, hi_end = 40, 1e-4, 0.999
    lo = lo_end
    f_lo = gap(lo)
    for i in range(1, grid + 1):
        hi = lo_end + (hi_end - lo_end) * i / grid
        f_hi = gap(hi)
        if (f_lo <= 0.0) != (f_hi <= 0.0):
            root = roots.brent(gap, lo, hi, tolerance=1e-10)
            if root is not None:
                return root
        lo, f_lo = hi, f_hi
    raise ValueError("no implied correlation found")


@dataclasses.dataclass(frozen=True)
class BasketTerms:
    """An nth-to-default basket of equal names."""

    names: int = 10
    nth: int = 1
    hazard: float = 0.01
    recovery: float = 0.4
    correlation: float = 0.3
    maturity: float = 5.0
    frequency: int = 4


@dataclasses.dataclass(frozen=True)
class BasketResult:
    """Spread and its Monte Carlo standard error."""

    spread: float
    std_error: float


def nth_to_default_spread(
    terms: BasketTerms, rate: float, paths: int, seed: int
) -> BasketResult:
    """nth-to-default spread by Monte Carlo with a flat discount rate.

    The NumPy stream differs from the C++ library's, so results agree
    only within Monte Carlo error.

    Raises:
        ValueError: For invalid terms or paths < 2.
    """
    t = terms
    if (
        t.names < 1
        or not 1 <= t.nth <= t.names
        or not t.hazard > 0.0
        or not 0.0 <= t.correlation < 1.0
        or not t.maturity > 0.0
        or t.frequency < 1
        or paths < 2
    ):
        raise ValueError("invalid basket terms")
    rng = np.random.default_rng(seed)
    f = rng.standard_normal((paths, 1))
    eps = rng.standard_normal((paths, t.names))
    u = special.ndtr(
        math.sqrt(t.correlation) * f + math.sqrt(1.0 - t.correlation) * eps
    )
    tau = -np.log1p(-u) / t.hazard
    nth = np.partition(tau, t.nth - 1, axis=1)[:, t.nth - 1]
    stop = np.minimum(nth, t.maturity)
    dt = 1.0 / t.frequency
    max_periods = math.floor(t.maturity * t.frequency + 1e-9)
    paid = np.minimum(np.floor((stop + 1e-12) / dt), max_periods).astype(int)
    pay_times = dt * np.arange(1, max_periods + 1)
    cumulative = np.concatenate(
        ([0.0], np.cumsum(dt * np.exp(-rate * pay_times)))
    )
    annuity = cumulative[paid]
    defaulted = nth < t.maturity
    disc_stop = np.exp(-rate * stop)
    annuity = annuity + np.where(defaulted, (stop - paid * dt) * disc_stop, 0.0)
    protection = np.where(defaulted, (1.0 - t.recovery) * disc_stop, 0.0)
    mean_annuity = float(annuity.mean())
    std_error = float(protection.std(ddof=1)) / math.sqrt(paths)
    return BasketResult(
        float(protection.mean()) / mean_annuity, std_error / mean_annuity
    )
