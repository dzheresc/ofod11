"""Exposure profiles, CVA, DVA and FVA (Hull, Ch 9)."""

from collections.abc import Sequence
import dataclasses
import math

import numpy as np

from ofod.curves import credit_curve as cc


@dataclasses.dataclass(frozen=True)
class ExposureProfile:
    """Discounted EE, ENE and PFE at each time."""

    times: list[float]
    expected_exposure: list[float]
    expected_negative_exposure: list[float]
    potential_future_exposure: list[float]


def profile_from_paths(
    times: Sequence[float],
    values: Sequence[Sequence[float]],
    pfe_quantile: float = 0.975,
    threshold: float = 1e300,
) -> ExposureProfile:
    """Exposure statistics from simulated (discounted) values.

    Exposure is min(max(V, 0), threshold): a collateral threshold H caps
    what can be lost.

    Raises:
        ValueError: If fewer than 2 paths or a path has the wrong length.
    """
    if len(values) < 2 or any(len(p) != len(times) for p in values):
        raise ValueError("need >= 2 paths, each one value per time")
    v = np.asarray(values, dtype=float)
    pos = np.minimum(np.maximum(v, 0.0), threshold)
    neg = np.minimum(np.maximum(-v, 0.0), threshold)
    return ExposureProfile(
        [float(t) for t in times],
        pos.mean(axis=0).tolist(),
        neg.mean(axis=0).tolist(),
        np.quantile(pos, pfe_quantile, axis=0).tolist(),
    )


def simulate_forward_exposure(
    spot: float,
    strike: float,
    rate: float,
    volatility: float,
    maturity: float,
    steps: int,
    paths: int,
    seed: int,
    pfe_quantile: float = 0.975,
    threshold: float = 1e300,
) -> ExposureProfile:
    """Exposure of a long forward on a GBM asset (discounted to today).

    The NumPy stream differs from the C++ library's.

    Raises:
        ValueError: For steps < 1, paths < 2 or non-positive spot/maturity.
    """
    if steps < 1 or paths < 2 or not maturity > 0.0 or not spot > 0.0:
        raise ValueError("invalid simulation inputs")
    dt = maturity / steps
    times = dt * np.arange(1, steps + 1)
    z = np.random.default_rng(seed).standard_normal((paths, steps))
    increments = (rate - 0.5 * volatility**2) * dt + volatility * math.sqrt(
        dt
    ) * z
    s = spot * np.exp(np.cumsum(increments, axis=1))
    value = (s - strike * np.exp(-rate * (maturity - times))) * np.exp(
        -rate * times
    )
    return profile_from_paths(
        times.tolist(), value.tolist(), pfe_quantile, threshold
    )


def cva(
    times: Sequence[float],
    exposures: Sequence[float],
    curve: cc.CreditCurve,
    recovery: float,
) -> float:
    """CVA = (1 - R) sum q_i v_i (eq. 9.1).

    Raises:
        ValueError: If the sizes differ.
    """
    if len(times) != len(exposures):
        raise ValueError("times and exposures differ in size")
    total, start = 0.0, 0.0
    for t, v in zip(times, exposures, strict=True):
        total += curve.default_probability(start, t) * v
        start = t
    return (1.0 - recovery) * total


def dva(
    times: Sequence[float],
    negative_exposures: Sequence[float],
    own_curve: cc.CreditCurve,
    own_recovery: float,
) -> float:
    """DVA: CVA's formula on ENE with the bank's own default curve."""
    return cva(times, negative_exposures, own_curve, own_recovery)


def funding_value_adjustment(
    times: Sequence[float], exposures: Sequence[float], funding_spread: float
) -> float:
    """FVA = sum s dt_i v_i on the funded exposure.

    Raises:
        ValueError: If the sizes differ.
    """
    if len(times) != len(exposures):
        raise ValueError("times and exposures differ in size")
    total, start = 0.0, 0.0
    for t, v in zip(times, exposures, strict=True):
        total += funding_spread * (t - start) * v
        start = t
    return total
