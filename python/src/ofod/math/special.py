"""Incomplete gamma and noncentral chi-square (CEV model, Hull Ch 27.1)."""

import math

from scipy import special
from scipy import stats


def regularized_gamma_p(a: float, x: float) -> float:
    """P(a, x) = gamma(a, x) / Gamma(a); NaN for invalid arguments."""
    if not (a > 0.0 and x >= 0.0):
        return math.nan
    return float(special.gammainc(a, x))


def noncentral_chi_square_cdf(x: float, dof: float, lambda_: float) -> float:
    """P(X <= x) for a noncentral chi-square; NaN for invalid arguments."""
    if not (dof > 0.0 and lambda_ >= 0.0):
        return math.nan
    if x <= 0.0:
        return 0.0
    if lambda_ == 0.0:
        return regularized_gamma_p(0.5 * dof, 0.5 * x)
    return float(stats.ncx2.cdf(x, dof, lambda_))
