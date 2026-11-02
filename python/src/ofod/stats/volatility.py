"""Volatility and correlation models (Hull, Ch 23).

EWMA, GARCH(1,1) with a maximum likelihood fit, the Ljung-Box test and a
positive semi-definite check. Returns are daily and variances per day.
"""

from collections.abc import Sequence
import dataclasses
import math
import sys

from ofod.math import linalg
from ofod.math import optimize


def ewma_variance_update(variance: float, ret: float, lambda_: float) -> float:
    """sigma_n^2 = lambda sigma_{n-1}^2 + (1 - lambda) u_{n-1}^2."""
    return lambda_ * variance + (1.0 - lambda_) * ret * ret


def ewma_covariance_update(
    covariance: float, ret_x: float, ret_y: float, lambda_: float
) -> float:
    """cov_n = lambda cov_{n-1} + (1 - lambda) x_{n-1} y_{n-1}."""
    return lambda_ * covariance + (1.0 - lambda_) * ret_x * ret_y


def ewma_variances(
    returns: Sequence[float], lambda_: float, initial: float
) -> list[float]:
    """v[0] = initial and v[i] uses returns[0..i-1]; length n + 1."""
    v = [initial]
    for u in returns:
        v.append(ewma_variance_update(v[-1], u, lambda_))
    return v


def _gaussian_log_likelihood(
    returns: Sequence[float], v: Sequence[float]
) -> float:
    ll = 0.0
    for i in range(1, len(returns)):
        if not v[i] > 0.0:
            return -math.inf
        ll += -math.log(v[i]) - returns[i] * returns[i] / v[i]
    return ll


@dataclasses.dataclass(frozen=True)
class Garch11:
    """sigma_n^2 = omega + alpha u_{n-1}^2 + beta sigma_{n-1}^2."""

    omega: float = 0.0
    alpha: float = 0.0
    beta: float = 0.0

    def long_run_variance(self) -> float:
        """V_L = omega / (1 - alpha - beta)."""
        return self.omega / (1.0 - self.alpha - self.beta)

    def update(self, variance: float, ret: float) -> float:
        """One GARCH step (eq. 23.9)."""
        return self.omega + self.alpha * ret * ret + self.beta * variance

    def expected_variance(self, variance: float, days: float) -> float:
        """E[sigma_{n+t}^2] (eq. 23.14)."""
        vl = self.long_run_variance()
        return vl + math.pow(self.alpha + self.beta, days) * (variance - vl)

    def term_structure_volatility(
        self, variance: float, days: float, days_per_year: float = 252.0
    ) -> float:
        """Annualised volatility for an option lasting `days` (eq. 23.15)."""
        vl = self.long_run_variance()
        a = math.log(1.0 / (self.alpha + self.beta))
        factor = (1.0 - math.exp(-a * days)) / (a * days)
        return math.sqrt(days_per_year * (vl + factor * (variance - vl)))

    def variances(self, returns: Sequence[float]) -> list[float]:
        """Hull's convention: v[1] = u[0]^2 (and v[0] set to u[0]^2)."""
        n = len(returns)
        v = [0.0] * n
        if n == 0:
            return v
        v[0] = returns[0] * returns[0]
        if n > 1:
            v[1] = v[0]
        for i in range(2, n):
            v[i] = self.update(v[i - 1], returns[i - 1])
        return v

    def log_likelihood(self, returns: Sequence[float]) -> float:
        """sum_{i >= 1} (-ln v_i - u_i^2 / v_i) (Table 23.1)."""
        return _gaussian_log_likelihood(returns, self.variances(returns))


def fit_garch11(returns: Sequence[float]) -> Garch11:
    """Maximum likelihood GARCH(1,1) by Nelder-Mead.

    Raises:
        ValueError: For fewer than 10 returns, a zero sample variance or a
            fit that does not converge.
    """
    if len(returns) < 10:
        raise ValueError("need at least 10 returns")
    sum_sq = 0.0
    for u in returns:
        sum_sq += u * u
    s2 = sum_sq / len(returns)
    if not s2 > 0.0:
        raise ValueError("returns have zero variance")

    def objective(x: Sequence[float]) -> float:
        g = Garch11(x[0] * s2, x[1], x[2])
        if (
            x[0] <= 0.0
            or g.alpha < 0.0
            or g.beta < 0.0
            or g.alpha + g.beta >= 1
        ):
            return sys.float_info.max
        ll = g.log_likelihood(returns)
        return -ll if math.isfinite(ll) else sys.float_info.max

    m = optimize.nelder_mead(
        objective,
        [0.05, 0.10, 0.85],
        [0.02, 0.05, 0.05],
        optimize.NelderMeadOptions(f_tol=1e-9, x_tol=1e-9),
    )
    if not m.converged:
        raise ValueError("GARCH fit did not converge")
    return Garch11(m.x[0] * s2, m.x[1], m.x[2])


def ewma_log_likelihood(returns: Sequence[float], lambda_: float) -> float:
    """EWMA log likelihood with v[1] = u[0]^2."""
    n = len(returns)
    if n < 2:
        return 0.0
    v = [0.0] * n
    v[0] = returns[0] * returns[0]
    v[1] = v[0]
    for i in range(2, n):
        v[i] = ewma_variance_update(v[i - 1], returns[i - 1], lambda_)
    return _gaussian_log_likelihood(returns, v)


def autocorrelation(x: Sequence[float], lag: int) -> float:
    """Lag-k sample autocorrelation.

    Raises:
        ValueError: Unless 0 < lag < n and x is not constant.
    """
    n = len(x)
    if lag <= 0 or lag >= n:
        raise ValueError("need 0 < lag < len(x)")
    mean = 0.0
    for v in x:
        mean += v
    mean /= n
    num = den = 0.0
    for i in range(n):
        d = x[i] - mean
        den += d * d
        if i >= lag:
            num += d * (x[i - lag] - mean)
    if den == 0.0:
        raise ValueError("series is constant")
    return num / den


def ljung_box(x: Sequence[float], lags: int) -> float:
    """Q = n (n + 2) sum rho_k^2 / (n - k) (Ch 23.5).

    Raises:
        ValueError: Unless 0 < lags < n.
    """
    n = float(len(x))
    if lags <= 0 or lags >= len(x):
        raise ValueError("need 0 < lags < len(x)")
    q = 0.0
    for k in range(1, lags + 1):
        rho = autocorrelation(x, k)
        q += rho * rho / (n - k)
    return n * (n + 2.0) * q


def is_positive_semidefinite(
    a: Sequence[Sequence[float]], tolerance: float = 1e-12
) -> bool:
    """True if a symmetric matrix has no eigenvalue below -tolerance."""
    try:
        return linalg.symmetric_eigen(a).values[-1] >= -tolerance
    except ValueError:
        return False
