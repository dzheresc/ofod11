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
