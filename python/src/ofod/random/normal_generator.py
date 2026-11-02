"""Seeded standard normal draws and Cholesky correlation (Hull, Ch 14).

The stream comes from NumPy and differs from the C++ library's
std::mt19937_64; models take explicit shocks so both can be compared.
"""

from collections.abc import Sequence
import math

import numpy as np

Matrix = list[list[float]]


class NormalGenerator:
    """Reproducible N(0, 1) draws."""

    def __init__(self, seed: int) -> None:
        """Creates a stream seeded with `seed`."""
        self._rng = np.random.default_rng(seed)

    def next(self) -> float:
        """Returns one draw."""
        return float(self._rng.standard_normal())

    def sample(self, n: int) -> list[float]:
        """Returns `n` draws."""
        return [float(x) for x in self._rng.standard_normal(max(n, 0))]


def cholesky(a: Sequence[Sequence[float]]) -> Matrix:
    """Returns lower-triangular L with L L^T = a.

    Raises:
        ValueError: Unless `a` is square, symmetric and positive definite.
    """
    n = len(a)
    if any(len(row) != n for row in a):
        raise ValueError("matrix must be square")
    lower = [[0.0] * n for _ in range(n)]
    for i in range(n):
        for j in range(i + 1):
            if abs(a[i][j] - a[j][i]) > 1e-12:
                raise ValueError("matrix must be symmetric")
            total = a[i][j] - sum(lower[i][k] * lower[j][k] for k in range(j))
            if i == j:
                if total <= 0.0:
                    raise ValueError("matrix must be positive definite")
                lower[i][i] = math.sqrt(total)
            else:
                lower[i][j] = total / lower[j][j]
    return lower


def correlate(
    lower: Sequence[Sequence[float]], z: Sequence[float]
) -> list[float]:
    """Returns L z, independent normals made correlated (Ch 14.5)."""
    return [
        sum(lower[i][k] * z[k] for k in range(min(i + 1, len(z))))
        for i in range(len(lower))
    ]
