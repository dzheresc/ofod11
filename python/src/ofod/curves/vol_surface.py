"""Implied volatility surface on a (maturity x strike) grid (Ch 20.6)."""

import bisect
from collections.abc import Sequence
import enum
import math


class VolInterpolation(enum.Enum):
    """How the surface interpolates across maturities."""

    TOTAL_VARIANCE = "total_variance"  # Linear in sigma^2 T.
    LINEAR_VOL = "linear_vol"  # Linear in sigma (Hull's Table 20.2).


def _strictly_increasing(xs: Sequence[float]) -> bool:
    return len(xs) > 0 and all(a < b for a, b in zip(xs, xs[1:], strict=False))


class VolSurface:
    """`volatilities[i][j]` is the vol for maturities[i] and strikes[j]."""

    def __init__(
        self,
        maturities: Sequence[float],
        strikes: Sequence[float],
        volatilities: Sequence[Sequence[float]],
        interpolation: VolInterpolation = VolInterpolation.TOTAL_VARIANCE,
    ) -> None:
        """Initializes the surface.

        Raises:
            ValueError: Unless both axes are non-empty and strictly
                increasing, maturities are positive, every row has one entry
                per strike and every volatility is positive.
        """
        if (
            not _strictly_increasing(maturities)
            or not _strictly_increasing(strikes)
            or maturities[0] <= 0.0
            or len(volatilities) != len(maturities)
            or any(len(row) != len(strikes) for row in volatilities)
            or any(not v > 0.0 for row in volatilities for v in row)
        ):
            raise ValueError("invalid volatility surface grid")
        self._maturities = [float(t) for t in maturities]
        self._strikes = [float(k) for k in strikes]
        self._vols = [[float(v) for v in row] for row in volatilities]
        self._interpolation = interpolation

    @property
    def maturities(self) -> list[float]:
        """Maturity axis."""
        return list(self._maturities)

    @property
    def strikes(self) -> list[float]:
        """Strike axis."""
        return list(self._strikes)

    @property
    def volatilities(self) -> list[list[float]]:
        """Volatility grid, one row per maturity."""
        return [list(row) for row in self._vols]

    @property
    def interpolation(self) -> VolInterpolation:
        """Interpolation across maturities."""
        return self._interpolation

    def _row_volatility(self, i: int, strike: float) -> float:
        row, ks = self._vols[i], self._strikes
        if strike <= ks[0]:
            return row[0]
        if strike >= ks[-1]:
            return row[-1]
        j = bisect.bisect_right(ks, strike) - 1
        w = (strike - ks[j]) / (ks[j + 1] - ks[j])
        return row[j] + w * (row[j + 1] - row[j])

    def volatility(self, strike: float, t: float) -> float:
        """Linear in strike, then across maturities; flat beyond the grid."""
        ts = self._maturities
        if t <= ts[0]:
            return self._row_volatility(0, strike)
        if t >= ts[-1]:
            return self._row_volatility(len(ts) - 1, strike)
        i = bisect.bisect_right(ts, t) - 1
        t0, t1 = ts[i], ts[i + 1]
        w = (t - t0) / (t1 - t0)
        v0 = self._row_volatility(i, strike)
        v1 = self._row_volatility(i + 1, strike)
        if self._interpolation is VolInterpolation.LINEAR_VOL:
            return v0 + w * (v1 - v0)
        var0, var1 = v0 * v0 * t0, v1 * v1 * t1
        return math.sqrt((var0 + w * (var1 - var0)) / t)
