"""Forward-rate models (Hull, Ch 33).

The discrete HJM model and the LIBOR market model simulated under the
rolling spot measure, with the caplet-volatility bootstrap and Rebonato's
swaption volatility. Paths are vectorised: an `LmmPaths` holds every path
and a payoff returns one value per path.
"""

from collections.abc import Callable
from collections.abc import Sequence
import dataclasses
import math

import numpy as np

from ofod.instruments.option import OptionType
from ofod.pricers import monte_carlo

Matrix = list[list[float]]


@dataclasses.dataclass(frozen=True)
class LmmSpec:
    """Forwards F_k for [t_k, t_k+1] and stationary factor loadings.

    loadings[i] are the factor volatilities of a forward rate whose reset
    is i + 1 accrual periods away (Lambda_i, Ch 33.2).
    """

    accrual: float
    forwards: Sequence[float]
    loadings: Sequence[Sequence[float]]


def lmm_volatilities_from_caplets(caplet_vols: Sequence[float]) -> list[float]:
    """Returns Lambda_i from caplet vols for resets t_k = k accrual.

    Raises:
        ValueError: If an implied variance would be negative.
    """
    out = []
    cumulative = 0.0
    for k, vol in enumerate(caplet_vols):
        total = vol * vol * (k + 1)
        if total < cumulative:
            raise ValueError("caplet vols imply negative variance")
        out.append(math.sqrt(total - cumulative))
        cumulative = total
    return out


def one_factor_loadings(lambdas: Sequence[float]) -> Matrix:
    """Returns single-factor loadings."""
    return [[x] for x in lambdas]


def lmm_discount_factors(spec: LmmSpec) -> list[float]:
    """Returns P(0, t_k), k = 0..n."""
    p = [1.0]
    for f in spec.forwards:
        p.append(p[-1] / (1.0 + spec.accrual * f))
    return p


class LmmPaths:
    """Simulated forwards and numeraires for all paths."""

    def __init__(self, forwards: np.ndarray, numeraire: np.ndarray) -> None:
        """Wraps forwards[j, k, path] and numeraire[j, path]."""
        self._forwards = forwards
        self._numeraire = numeraire

    @property
    def size(self) -> int:
        """Number of forward rates n."""
        return int(self._numeraire.shape[0]) - 1

    def forward(self, j: int, k: int) -> np.ndarray:
        """Returns F_k(t_j) for j <= k on every path."""
        return np.asarray(self._forwards[j, k])

    def reset(self, k: int) -> np.ndarray:
        """Returns F_k(t_k)."""
        return np.asarray(self._forwards[k, k])

    def numeraire(self, j: int) -> np.ndarray:
        """Returns the rolled-over bank account at t_j."""
        return np.asarray(self._numeraire[j])


LmmPayoff = Callable[[LmmPaths], np.ndarray]


@dataclasses.dataclass(frozen=True)
class LmmOptions:
    """Paths, seed and the predictor-corrector switch."""

    paths: int = 10_000
    seed: int = 1
    predictor_corrector: bool = False


def _validate(spec: LmmSpec) -> np.ndarray:
    n = len(spec.forwards)
    if not spec.accrual > 0.0 or n == 0 or len(spec.loadings) + 1 < n:
        raise ValueError("inconsistent LMM spec")
    if n == 1:
        return np.zeros((0, 1))
    widths = {len(row) for row in spec.loadings}
    if len(widths) != 1 or 0 in widths:
        raise ValueError("loadings rows must share a positive width")
    return np.asarray(spec.loadings, dtype=float)


def _drifts(lam: np.ndarray, tau: float, j: int, f: np.ndarray) -> np.ndarray:
    """Returns mu[k, path] for k = j+1..n-1 (rows below j+1 are zero)."""
    n = f.shape[0]
    mu = np.zeros_like(f)
    total = np.zeros((lam.shape[1], f.shape[1]))
    for k in range(j + 1, n):
        row = lam[k - j - 1]
        weight = tau * f[k] / (1.0 + tau * f[k])
        total += np.outer(row, weight)
        mu[k] = row @ total - 0.5 * float(row @ row)
    return mu


def lmm_monte_carlo(
    spec: LmmSpec, payoff: LmmPayoff, options: LmmOptions | None = None
) -> monte_carlo.McResult:
    """Prices `payoff` under the spot measure with Hull's log-Euler step.

    Raises:
        ValueError: For an inconsistent spec or no paths.
    """
    options = options or LmmOptions()
    lam = _validate(spec)
    if options.paths <= 0:
        raise ValueError("need paths")
    n = len(spec.forwards)
    m = options.paths
    tau = spec.accrual
    rng = np.random.default_rng(options.seed)
    forwards = np.zeros((n, n, m))
    numeraire = np.ones((n + 1, m))
    f = np.repeat(np.asarray(spec.forwards, dtype=float)[:, None], m, axis=1)
    forwards[0] = f
    for j in range(n - 1):
        eps = rng.standard_normal((lam.shape[1], m))
        mu = _drifts(lam, tau, j, f)
        shock = np.zeros_like(f)
        for k in range(j + 1, n):
            shock[k] = lam[k - j - 1] @ eps * math.sqrt(tau)
        if options.predictor_corrector:
            predicted = f * np.exp(mu * tau + shock)
            mu = 0.5 * (mu + _drifts(lam, tau, j, predicted))
        numeraire[j + 1] = numeraire[j] * (1.0 + tau * f[j])
        live = slice(j + 1, n)
        f = f.copy()
        f[live] *= np.exp(mu[live] * tau + shock[live])
        forwards[j + 1] = f
    numeraire[n] = numeraire[n - 1] * (1.0 + tau * f[n - 1])
    values = np.asarray(payoff(LmmPaths(forwards, numeraire)), dtype=float)
    return monte_carlo.McResult(
        float(values.mean()), float(values.std()) / math.sqrt(m), m
    )


def rebonato_swaption_volatility(spec: LmmSpec, start: int, end: int) -> float:
    """Returns Rebonato's frozen-weight swaption volatility (Ch 33.2).

    Raises:
        ValueError: Unless 1 <= start < end <= n.
    """
    lam = _validate(spec)
    n = len(spec.forwards)
    if start < 1 or end > n or start >= end:
        raise ValueError("bad swap indices")
    tau = spec.accrual
    p = lmm_discount_factors(spec)
    annuity = sum(tau * p[k + 1] for k in range(start, end))
    s0 = (p[start] - p[end]) / annuity
    variance = 0.0
    for k in range(start, end):
        wk = tau * p[k + 1] / annuity * spec.forwards[k]
        for ell in range(start, end):
            wl = tau * p[ell + 1] / annuity * spec.forwards[ell]
            cov = sum(
                tau * float(lam[k - j - 1] @ lam[ell - j - 1])
                for j in range(start)
            )
            variance += wk * wl * cov
    return math.sqrt(variance / (s0 * s0 * start * tau))


@dataclasses.dataclass(frozen=True)
class HjmSpec:
    """Forwards f(0, T_i) on T_i = i dt and a volatility v(t, T)."""

    dt: float
    forwards: Sequence[float]
    volatility: Callable[[float, float], float]


def hjm_zero_bond_option(
    spec: HjmSpec,
    option_type: OptionType,
    expiry: int,
    maturity: int,
    strike: float,
    paths: int,
    seed: int,
) -> monte_carlo.McResult:
    """Prices a zero-coupon bond option by discrete HJM (Ch 33.1).

    Raises:
        ValueError: For steps outside the grid.
    """
    m = len(spec.forwards)
    if (
        not spec.dt > 0.0
        or expiry < 0
        or maturity > m
        or expiry > maturity
        or paths <= 0
    ):
        raise ValueError("bad HJM inputs")
    dt = spec.dt
    phi = 1.0 if option_type is OptionType.CALL else -1.0
    rng = np.random.default_rng(seed)
    f = np.repeat(np.asarray(spec.forwards, dtype=float)[:, None], paths, 1)
    integral = np.zeros(paths)
    for j in range(expiry):
        integral += f[j] * dt
        eps = rng.standard_normal(paths)
        v = np.array(
            [spec.volatility(j * dt, i * dt) for i in range(j + 1, maturity)]
        )
        u_next = np.cumsum(v) * dt
        u = u_next - v * dt
        drift = 0.5 * (u_next**2 - u**2)
        f[j + 1 : maturity] += (
            drift[:, None] + (v * math.sqrt(dt))[:, None] * eps[None, :]
        )
    log_bond = -f[expiry:maturity].sum(axis=0) * dt
    values = np.exp(-integral) * np.maximum(
        phi * (np.exp(log_bond) - strike), 0.0
    )
    return monte_carlo.McResult(
        float(values.mean()), float(values.std()) / math.sqrt(paths), paths
    )
