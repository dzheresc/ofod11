"""Value at risk and expected shortfall (Hull, Ch 22).

Losses are positive numbers; VaR and ES are reported as positive losses.
"""

from collections.abc import Callable
from collections.abc import Sequence
import dataclasses
import math

from ofod.math import linalg
from ofod.math import normal
from ofod.random import normal_generator

Matrix = list[list[float]]


def normal_var(sd: float, confidence: float, mean: float = 0.0) -> float:
    """VaR of a normal loss: mean + sd N^{-1}(X)."""
    return mean + sd * normal.norm_inv(confidence)


def normal_expected_shortfall(
    sd: float, confidence: float, mean: float = 0.0
) -> float:
    """ES of a normal loss: mean + sd phi(N^{-1}(X)) / (1 - X)."""
    return mean + sd * normal.norm_pdf(normal.norm_inv(confidence)) / (
        1.0 - confidence
    )


def scale_to_horizon(one_day: float, days: float) -> float:
    """T-day VaR = 1-day VaR * sqrt(T) (eq. 22.1)."""
    return one_day * math.sqrt(days)


def _tail_count(n: int, confidence: float) -> int:
    x = n * (1.0 - confidence)
    # Round half away from zero, like std::lround.
    return int(math.floor(x + 0.5)) if x >= 0 else -int(math.floor(-x + 0.5))


def historical_var(losses: Sequence[float], confidence: float) -> float:
    """k-th worst loss, k = round(n (1 - X)) (Hull's convention).

    Raises:
        ValueError: If k < 1.
    """
    k = _tail_count(len(losses), confidence)
    if k < 1:
        raise ValueError("too few scenarios for this confidence level")
    return sorted(losses, reverse=True)[k - 1]


def historical_expected_shortfall(
    losses: Sequence[float], confidence: float
) -> float:
    """Mean of the k worst losses.

    Raises:
        ValueError: If k < 1.
    """
    k = _tail_count(len(losses), confidence)
    if k < 1:
        raise ValueError("too few scenarios for this confidence level")
    worst = sorted(losses, reverse=True)
    total = 0.0
    for i in range(k):
        total += worst[i]
    return total / k


def volatility_adjusted_changes(
    changes: Sequence[float], vols: Sequence[float], current_vol: float
) -> list[float]:
    """changes[i] * current_vol / vols[i] (Ch 22.3).

    Raises:
        ValueError: For a size mismatch or a non-positive volatility.
    """
    if len(changes) != len(vols):
        raise ValueError("changes and vols differ in length")
    if any(not v > 0.0 for v in vols):
        raise ValueError("volatilities must be positive")
    return [c * current_vol / v for c, v in zip(changes, vols, strict=True)]


def portfolio_std_dev(
    exposures: Sequence[float],
    vols: Sequence[float],
    correlation: Sequence[Sequence[float]],
) -> float:
    """Standard deviation of sum alpha_i dx_i / x_i (eq. 22.3).

    Raises:
        ValueError: For inconsistent sizes.
    """
    n = len(exposures)
    if len(vols) != n or len(correlation) != n:
        raise ValueError("inconsistent sizes")
    var = 0.0
    for i in range(n):
        if len(correlation[i]) != n:
            raise ValueError("correlation matrix must be n x n")
        for j in range(n):
            var += (
                exposures[i]
                * exposures[j]
                * vols[i]
                * vols[j]
                * correlation[i][j]
            )
    return math.sqrt(max(var, 0.0))


@dataclasses.dataclass(frozen=True)
class Moments:
    """Mean, standard deviation and skewness of a P&L distribution."""

    mean: float
    std_dev: float
    skewness: float


def delta_gamma_moments(
    delta: float, gamma: float, sd_change: float
) -> Moments:
    """Moments of delta dx + gamma dx^2 / 2, dx ~ N(0, sd^2) (Ch 22.6)."""
    s2 = sd_change * sd_change
    mean = 0.5 * gamma * s2
    e2 = delta * delta * s2 + 0.75 * gamma * gamma * s2 * s2
    e3 = (
        4.5 * delta * delta * gamma * s2 * s2
        + 1.875 * gamma * gamma * gamma * s2 * s2 * s2
    )
    var = e2 - mean * mean
    sd = math.sqrt(var)
    third = e3 - 3.0 * e2 * mean + 2.0 * mean * mean * mean
    return Moments(mean, sd, third / (sd * sd * sd))


def cornish_fisher_var(pnl: Moments, confidence: float) -> float:
    """VaR from the Cornish-Fisher (1 - X) quantile of the P&L."""
    z = normal.norm_inv(1.0 - confidence)
    w = z + (z * z - 1.0) * pnl.skewness / 6.0
    return -(pnl.mean + w * pnl.std_dev)


@dataclasses.dataclass(frozen=True)
class VarEstimate:
    """VaR and expected shortfall."""

    var: float
    expected_shortfall: float


PnlFunction = Callable[[Sequence[float]], float]


def monte_carlo_var(
    pnl: PnlFunction,
    covariance: Sequence[Sequence[float]],
    confidence: float,
    paths: int,
    seed: int,
) -> VarEstimate:
    """Monte Carlo VaR with full revaluation (Ch 22.6).

    Draws come from NumPy, so results match the C++ library only
    statistically.

    Raises:
        ValueError: If the covariance is not positive definite or there are
            too few paths for the confidence level.
    """
    if paths < 1 or _tail_count(paths, confidence) < 1:
        raise ValueError("too few paths for this confidence level")
    lower = normal_generator.cholesky(covariance)
    rng = normal_generator.NormalGenerator(seed)
    dims = len(covariance)
    losses = []
    for _ in range(paths):
        z = rng.sample(dims)
        losses.append(-pnl(normal_generator.correlate(lower, z)))
    return VarEstimate(
        historical_var(losses, confidence),
        historical_expected_shortfall(losses, confidence),
    )


def _xlogy(x: float, y: float) -> float:
    return 0.0 if x == 0.0 else x * math.log(y)


def kupiec_statistic(observations: int, exceptions: int, p: float) -> float:
    """Kupiec's likelihood ratio; compare with chi-square(1) (Ch 22.8)."""
    n, m = float(observations), float(exceptions)
    q = m / n
    null_ll = _xlogy(n - m, 1.0 - p) + _xlogy(m, p)
    alt_ll = _xlogy(n - m, 1.0 - q) + _xlogy(m, q)
    return -2.0 * (null_ll - alt_ll)


def binomial_tail_probability(n: int, m: int, p: float) -> float:
    """P(X >= m) for X ~ Binomial(n, p)."""
    if m <= 0:
        return 1.0
    if m > n:
        return 0.0
    below = 0.0
    for k in range(m):
        log_term = (
            math.lgamma(n + 1.0)
            - math.lgamma(k + 1.0)
            - math.lgamma(n - k + 1.0)
            + k * math.log(p)
            + (n - k) * math.log1p(-p)
        )
        below += math.exp(log_term)
    return max(0.0, 1.0 - below)


@dataclasses.dataclass(frozen=True)
class PcaResult:
    """Factor variances (descending) and loadings[k] for factor k."""

    variances: list[float]
    loadings: Matrix


def principal_components(observations: Sequence[Sequence[float]]) -> PcaResult:
    """PCA of the sample covariance; rows are dates (Ch 22.9).

    Raises:
        ValueError: For fewer than two rows or ragged rows.
    """
    rows = len(observations)
    if rows < 2:
        raise ValueError("need at least two observations")
    cols = len(observations[0])
    if cols == 0 or any(len(r) != cols for r in observations):
        raise ValueError("rows must be non-empty and equally long")
    mean = [0.0] * cols
    for row in observations:
        for j in range(cols):
            mean[j] += row[j]
    mean = [m / rows for m in mean]
    cov = [[0.0] * cols for _ in range(cols)]
    for row in observations:
        for i in range(cols):
            for j in range(i, cols):
                cov[i][j] += (row[i] - mean[i]) * (row[j] - mean[j])
    for i in range(cols):
        for j in range(i, cols):
            cov[i][j] /= rows - 1
            cov[j][i] = cov[i][j]
    eigen = linalg.symmetric_eigen(cov)
    return PcaResult(eigen.values, eigen.vectors)


def pca_std_dev(
    exposures: Sequence[float],
    loadings: Sequence[Sequence[float]],
    factor_sds: Sequence[float],
) -> float:
    """Standard deviation of a portfolio driven by the given factors.

    Raises:
        ValueError: For inconsistent sizes.
    """
    if len(loadings) != len(factor_sds):
        raise ValueError("one sd per factor")
    var = 0.0
    for k, row in enumerate(loadings):
        if len(row) != len(exposures):
            raise ValueError("loadings must match exposures")
        exposure = 0.0
        for i in range(len(exposures)):
            exposure += exposures[i] * row[i]
        var += exposure * exposure * factor_sds[k] * factor_sds[k]
    return math.sqrt(var)
