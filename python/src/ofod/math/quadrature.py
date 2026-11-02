"""Gauss-Hermite quadrature (copula factor integration, Ch 24-25)."""

import dataclasses
import math


@dataclasses.dataclass(frozen=True)
class Quadrature:
    """Nodes (descending) and weights."""

    nodes: list[float]
    weights: list[float]


def gauss_hermite(n: int) -> Quadrature:
    """Rule with sum w_i f(x_i) ~ int e^{-x^2} f(x) dx.

    Raises:
        ValueError: For n < 1, n > 200 or no Newton convergence.
    """
    if n < 1 or n > 200:
        raise ValueError("need 1 <= n <= 200")
    pim4 = 1.0 / math.pow(math.pi, 0.25)
    x = [0.0] * n
    w = [0.0] * n
    z = 0.0
    for i in range((n + 1) // 2):
        if i == 0:
            z = math.sqrt(2.0 * n + 1.0) - 1.85575 * math.pow(
                2.0 * n + 1.0, -1.0 / 6.0
            )
        elif i == 1:
            z -= 1.14 * math.pow(n, 0.426) / z
        elif i == 2:
            z = 1.86 * z - 0.86 * x[0]
        elif i == 3:
            z = 1.91 * z - 0.91 * x[1]
        else:
            z = 2.0 * z - x[i - 2]
        pp = 0.0
        for _ in range(100):
            p1, p2 = pim4, 0.0
            for j in range(1, n + 1):
                p3 = p2
                p2 = p1
                p1 = z * math.sqrt(2.0 / j) * p2 - math.sqrt((j - 1.0) / j) * p3
            pp = math.sqrt(2.0 * n) * p2
            z1 = z
            z = z1 - p1 / pp
            if abs(z - z1) <= 3e-14:
                break
        else:
            raise ValueError("Gauss-Hermite Newton iteration did not converge")
        x[i], x[n - 1 - i] = z, -z
        w[i] = 2.0 / (pp * pp)
        w[n - 1 - i] = w[i]
    return Quadrature(x, w)


def gauss_hermite_normal(n: int) -> Quadrature:
    """The rule rescaled for E[f(Z)], Z ~ N(0, 1)."""
    q = gauss_hermite(n)
    return Quadrature(
        [v * math.sqrt(2.0) for v in q.nodes],
        [v / math.sqrt(math.pi) for v in q.weights],
    )
