"""Derivative-free minimisation (Nelder-Mead), matching the C++ library."""

from collections.abc import Callable
from collections.abc import Sequence
import dataclasses


@dataclasses.dataclass(frozen=True)
class Minimum:
    """Best point found, its value, iterations used and convergence flag."""

    x: list[float]
    value: float
    iterations: int
    converged: bool


@dataclasses.dataclass(frozen=True)
class NelderMeadOptions:
    """Stopping rules: both spreads small, or the iteration cap."""

    f_tol: float = 1e-12
    x_tol: float = 1e-10
    max_iterations: int = 10_000


Objective = Callable[[Sequence[float]], float]


def _affine(a: list[float], b: list[float], t: float) -> list[float]:
    return [ai + t * (bi - ai) for ai, bi in zip(a, b, strict=True)]


def nelder_mead(
    f: Objective,
    x0: Sequence[float],
    step: Sequence[float],
    options: NelderMeadOptions | None = None,
) -> Minimum:
    """Minimises `f` from the simplex x0 + step_i e_i.

    Uses reflection 1, expansion 2, contraction 1/2 and shrink 1/2.
    """
    options = options or NelderMeadOptions()
    n = len(x0)
    simplex = [[float(x) for x in x0] for _ in range(n + 1)]
    for i in range(n):
        simplex[i + 1][i] += step[i]
    values = [f(p) for p in simplex]
    iteration = 0
    converged = False
    while iteration < options.max_iterations:
        order = sorted(range(n + 1), key=lambda i: values[i])
        simplex = [simplex[i] for i in order]
        values = [values[i] for i in order]
        f_spread = max(abs(values[i] - values[0]) for i in range(1, n + 1))
        x_spread = max(
            abs(simplex[i][j] - simplex[0][j])
            for i in range(1, n + 1)
            for j in range(n)
        )
        if f_spread <= options.f_tol and x_spread <= options.x_tol:
            converged = True
            break
        centroid = [0.0] * n
        for i in range(n):
            for j in range(n):
                centroid[j] += simplex[i][j] / n
        worst = simplex[n]
        reflected = _affine(centroid, worst, -1.0)
        f_reflected = f(reflected)
        iteration += 1
        if f_reflected < values[0]:
            expanded = _affine(centroid, worst, -2.0)
            f_expanded = f(expanded)
            if f_expanded < f_reflected:
                simplex[n], values[n] = expanded, f_expanded
            else:
                simplex[n], values[n] = reflected, f_reflected
            continue
        if f_reflected < values[n - 1]:
            simplex[n], values[n] = reflected, f_reflected
            continue
        outside = f_reflected < values[n]
        contracted = (
            _affine(centroid, reflected, 0.5)
            if outside
            else _affine(centroid, worst, 0.5)
        )
        f_contracted = f(contracted)
        if f_contracted < (f_reflected if outside else values[n]):
            simplex[n], values[n] = contracted, f_contracted
            continue
        for i in range(1, n + 1):
            simplex[i] = _affine(simplex[0], simplex[i], 0.5)
            values[i] = f(simplex[i])
    best = min(range(n + 1), key=lambda i: values[i])
    return Minimum(simplex[best], values[best], iteration, converged)
