"""Standard normal distribution helpers (Hull, Ch 15.9)."""

import math

_INV_SQRT_2PI = 1.0 / math.sqrt(2.0 * math.pi)

# Abramowitz-Stegun 26.2.17 coefficients, as printed in Hull.
_GAMMA = 0.2316419
_A1 = 0.319381530
_A2 = -0.356563782
_A3 = 1.781477937
_A4 = -1.821255978
_A5 = 1.330274429


def norm_pdf(x: float) -> float:
    """Computes the standard normal probability density.

    Args:
        x: Point at which to evaluate the density.

    Returns:
        The density phi(x).
    """
    return math.exp(-0.5 * x * x) * _INV_SQRT_2PI


def norm_cdf(x: float) -> float:
    """Computes the standard normal cumulative distribution N(x).

    Args:
        x: Upper integration limit.

    Returns:
        N(x), computed with erfc to full double precision.
    """
    return 0.5 * math.erfc(-x / math.sqrt(2.0))


def norm_cdf_poly(x: float) -> float:
    """Computes Hull's polynomial approximation to N(x).

    Args:
        x: Upper integration limit.

    Returns:
        N(x) with absolute error below 7.5e-8.
    """
    if x < 0.0:
        return 1.0 - norm_cdf_poly(-x)
    k = 1.0 / (1.0 + _GAMMA * x)
    poly = k * (_A1 + k * (_A2 + k * (_A3 + k * (_A4 + k * _A5))))
    return 1.0 - norm_pdf(x) * poly


_INV_A = (
    -3.969683028665376e01,
    2.209460984245205e02,
    -2.759285104469687e02,
    1.383577518672690e02,
    -3.066479806614716e01,
    2.506628277459239e00,
)
_INV_B = (
    -5.447609879822406e01,
    1.615858368580409e02,
    -1.556989798598866e02,
    6.680131188771972e01,
    -1.328068155288572e01,
)
_INV_C = (
    -7.784894002430293e-03,
    -3.223964580411365e-01,
    -2.400758277161838e00,
    -2.549732539343734e00,
    4.374664141464968e00,
    2.938163982698783e00,
)
_INV_D = (
    7.784695709041462e-03,
    3.224671290700398e-01,
    2.445134137142996e00,
    3.754408661907416e00,
)
_INV_LOW = 0.02425


def norm_inv(p: float) -> float:
    """Returns x with N(x) = p (Acklam plus one Halley step).

    Returns -inf / +inf at p = 0 / 1 and NaN outside [0, 1].
    """
    if math.isnan(p) or p < 0.0 or p > 1.0:
        return math.nan
    if p == 0.0:
        return -math.inf
    if p == 1.0:
        return math.inf
    a, b, c, d = _INV_A, _INV_B, _INV_C, _INV_D
    if p < _INV_LOW or p > 1.0 - _INV_LOW:
        q = math.sqrt(-2.0 * math.log(p if p < _INV_LOW else 1.0 - p))
        x = (
            ((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]
        ) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0)
        if p > 1.0 - _INV_LOW:
            x = -x
    else:
        q = p - 0.5
        r = q * q
        x = (
            (
                ((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r
                + a[5]
            )
            * q
            / (
                ((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r
                + 1.0
            )
        )
    e = norm_cdf(x) - p
    u = e / norm_pdf(x)
    return x - u / (1.0 + 0.5 * x * u)
