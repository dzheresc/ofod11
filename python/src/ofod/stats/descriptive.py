"""Sample statistics (n - 1 denominators) and least-squares regression."""

from collections.abc import Sequence
import dataclasses
import math


@dataclasses.dataclass(frozen=True)
class Regression:
    """Result of regressing y on x: y = intercept + slope * x."""

    intercept: float
    slope: float
    r_squared: float


def mean(x: Sequence[float]) -> float:
    """Returns the arithmetic mean.

    Raises:
        ValueError: If `x` is empty.
    """
    if not x:
        raise ValueError("mean of an empty sample")
    return math.fsum(x) / len(x)


def covariance(x: Sequence[float], y: Sequence[float]) -> float:
    """Returns the sample covariance.

    Raises:
        ValueError: If the sizes differ or there are fewer than two points.
    """
    if len(x) != len(y) or len(x) < 2:
        raise ValueError("covariance needs two equal samples of size >= 2")
    mx = mean(x)
    my = mean(y)
    total = 0.0
    for a, b in zip(x, y, strict=True):
        total += (a - mx) * (b - my)
    return total / (len(x) - 1)


def variance(x: Sequence[float]) -> float:
    """Returns the sample variance.

    Raises:
        ValueError: If there are fewer than two points.
    """
    return covariance(x, x)


def std_dev(x: Sequence[float]) -> float:
    """Returns the sample standard deviation.

    Raises:
        ValueError: If there are fewer than two points.
    """
    return math.sqrt(variance(x))


def correlation(x: Sequence[float], y: Sequence[float]) -> float:
    """Returns the sample correlation coefficient.

    Raises:
        ValueError: If the samples are invalid or either is constant.
    """
    cov = covariance(x, y)
    sx = std_dev(x)
    sy = std_dev(y)
    if sx == 0.0 or sy == 0.0:
        raise ValueError("correlation of a constant sample")
    return cov / (sx * sy)


def linear_regression(x: Sequence[float], y: Sequence[float]) -> Regression:
    """Regresses `y` on `x` by ordinary least squares.

    Raises:
        ValueError: If the samples are invalid or `x` is constant.
    """
    cov = covariance(x, y)
    var_x = variance(x)
    if var_x == 0.0:
        raise ValueError("regression on a constant regressor")
    slope = cov / var_x
    intercept = mean(y) - slope * mean(x)
    var_y = variance(y)
    r_squared = 1.0 if var_y == 0.0 else slope * cov / var_y
    return Regression(intercept, slope, r_squared)


def log_returns(prices: Sequence[float]) -> list[float]:
    """Returns ln(S_i / S_{i-1}) for consecutive prices (Ch 15.4)."""
    return [math.log(b / a) for a, b in zip(prices, prices[1:], strict=False)]


def historical_volatility(
    prices: Sequence[float], periods_per_year: float
) -> float:
    """Returns the annualised volatility of log returns (Ch 15.4).

    Raises:
        ValueError: With fewer than three prices.
    """
    if len(prices) < 3:
        raise ValueError("need at least three prices")
    return std_dev(log_returns(prices)) * math.sqrt(periods_per_year)
