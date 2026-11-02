"""Structural and one-factor credit models (Hull, Ch 24.6-24.9)."""

import dataclasses
import math

from ofod.math import normal


@dataclasses.dataclass(frozen=True)
class MertonResult:
    """Implied firm value, its volatility, Q(default) and debt value."""

    asset_value: float
    asset_volatility: float
    default_probability: float
    debt_value: float


def _residual(
    v: float,
    sv: float,
    equity: float,
    equity_vol: float,
    debt: float,
    rate: float,
    t: float,
) -> tuple[float, float]:
    sqrt_t = math.sqrt(t)
    d1 = (math.log(v / debt) + (rate + 0.5 * sv * sv) * t) / (sv * sqrt_t)
    d2 = d1 - sv * sqrt_t
    n1 = normal.norm_cdf(d1)
    e = v * n1 - debt * math.exp(-rate * t) * normal.norm_cdf(d2)
    return e - equity, n1 * sv * v - equity_vol * equity


def merton_solve(
    equity: float, equity_vol: float, debt: float, rate: float, t: float
) -> MertonResult:
    """Solves eqs. 24.3 and 24.4 for V0 and sigma_V by Newton's method.

    Raises:
        ValueError: For non-positive inputs or if Newton fails.
    """
    if not (equity > 0.0 and equity_vol > 0.0 and debt > 0.0 and t > 0.0):
        raise ValueError("equity, vol, debt and t must be positive")
    v = equity + debt * math.exp(-rate * t)
    sv = equity_vol * equity / v
    args = (equity, equity_vol, debt, rate, t)
    for _ in range(100):
        fe, fs = _residual(v, sv, *args)
        if abs(fe) < 1e-12 * equity and abs(fs) < 1e-12 * equity * equity_vol:
            d2 = (math.log(v / debt) + (rate - 0.5 * sv * sv) * t) / (
                sv * math.sqrt(t)
            )
            return MertonResult(v, sv, normal.norm_cdf(-d2), v - equity)
        hv, hs = 1e-6 * v, 1e-6 * sv
        fv1 = _residual(v + hv, sv, *args)
        fv0 = _residual(v - hv, sv, *args)
        fs1 = _residual(v, sv + hs, *args)
        fs0 = _residual(v, sv - hs, *args)
        j11 = (fv1[0] - fv0[0]) / (2.0 * hv)
        j12 = (fs1[0] - fs0[0]) / (2.0 * hs)
        j21 = (fv1[1] - fv0[1]) / (2.0 * hv)
        j22 = (fs1[1] - fs0[1]) / (2.0 * hs)
        det = j11 * j22 - j12 * j21
        if det == 0.0 or not math.isfinite(det):
            raise ValueError("singular Merton Jacobian")
        dv = (fe * j22 - fs * j12) / det
        ds = (j11 * fs - j21 * fe) / det
        v = max(v - dv, 0.5 * v)
        sv = max(sv - ds, 0.5 * sv)
    raise ValueError("Merton solve did not converge")


def joint_default_probability(q1: float, q2: float, rho: float) -> float:
    """P(both default) under a Gaussian copula (Ch 24.8)."""
    return normal.bivariate_norm_cdf(
        normal.norm_inv(q1), normal.norm_inv(q2), rho
    )


def conditional_default_probability(
    q: float, rho: float, factor: float
) -> float:
    """Q(T | F) = N((N^-1(Q) - sqrt(rho) F) / sqrt(1 - rho)) (eq. 24.10)."""
    return normal.norm_cdf(
        (normal.norm_inv(q) - math.sqrt(rho) * factor) / math.sqrt(1.0 - rho)
    )


def wcdr(pd: float, rho: float, confidence: float) -> float:
    """Worst-case default rate at `confidence` (eq. 24.11)."""
    return normal.norm_cdf(
        (normal.norm_inv(pd) + math.sqrt(rho) * normal.norm_inv(confidence))
        / math.sqrt(1.0 - rho)
    )


def vasicek_credit_loss(
    exposure: float, pd: float, lgd: float, rho: float, confidence: float
) -> float:
    """Credit VaR of a large portfolio: L x LGD x WCDR (Ch 24.9)."""
    return exposure * lgd * wcdr(pd, rho, confidence)
