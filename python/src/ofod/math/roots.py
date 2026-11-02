"""One-dimensional root finders."""

from collections.abc import Callable

from scipy import optimize


def brent(
    f: Callable[[float], float],
    lo: float,
    hi: float,
    tolerance: float = 1e-12,
    max_iterations: int = 200,
) -> float | None:
    """Finds a root of `f` in [lo, hi] with Brent's method.

    Returns:
        The root, or None if the interval does not bracket a sign change or
        the iteration limit is reached.
    """
    f_lo, f_hi = f(lo), f(hi)
    if f_lo == 0.0:
        return lo
    if f_hi == 0.0:
        return hi
    if (f_lo > 0.0) == (f_hi > 0.0):
        return None
    root, result = optimize.brentq(
        f,
        lo,
        hi,
        xtol=tolerance,
        maxiter=max_iterations,
        full_output=True,
        disp=False,
    )
    return float(root) if result.converged else None


def newton(
    f: Callable[[float], float],
    df: Callable[[float], float],
    x0: float,
    tolerance: float = 1e-12,
    max_iterations: int = 200,
) -> float | None:
    """Finds a root of `f` by Newton-Raphson from `x0`.

    Returns:
        The root, or None on a zero derivative or at the iteration limit.
    """
    x = x0
    for _ in range(max_iterations):
        slope = df(x)
        if slope == 0.0:
            return None
        step = f(x) / slope
        x -= step
        if abs(step) < tolerance:
            return x
    return None
