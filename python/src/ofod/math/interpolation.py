"""Piecewise-linear interpolation with flat extrapolation."""

import bisect
from collections.abc import Sequence


class LinearInterpolator:
    """Linear interpolation through (x, y) points, flat outside them."""

    def __init__(self, xs: Sequence[float], ys: Sequence[float]) -> None:
        """Initializes the interpolator.

        Args:
            xs: Strictly increasing abscissas.
            ys: Values at `xs`.

        Raises:
            ValueError: If the inputs are empty, unequal in length or not
                strictly increasing.
        """
        if not xs or len(xs) != len(ys):
            raise ValueError("xs and ys must be non-empty and equal length")
        if any(b <= a for a, b in zip(xs, xs[1:], strict=False)):
            raise ValueError("xs must be strictly increasing")
        self._xs = [float(x) for x in xs]
        self._ys = [float(y) for y in ys]

    def __call__(self, x: float) -> float:
        """Returns the interpolated value at `x`."""
        xs, ys = self._xs, self._ys
        if x <= xs[0]:
            return ys[0]
        if x >= xs[-1]:
            return ys[-1]
        i = bisect.bisect_right(xs, x)
        w = (x - xs[i - 1]) / (xs[i] - xs[i - 1])
        return ys[i - 1] + w * (ys[i] - ys[i - 1])

    @property
    def xs(self) -> list[float]:
        """Abscissas."""
        return list(self._xs)

    @property
    def ys(self) -> list[float]:
        """Values."""
        return list(self._ys)
