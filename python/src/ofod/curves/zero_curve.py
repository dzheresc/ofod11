"""Zero-coupon yield curve (Hull, Ch 4.5 and 4.8)."""

from collections.abc import Sequence
import math

from ofod.math import interpolation


class ZeroCurve:
    """Continuously compounded zero rates at pillar times (years).

    Rates are linearly interpolated and flat outside the pillars, as in Hull.
    """

    def __init__(
        self, times: Sequence[float], zero_rates: Sequence[float]
    ) -> None:
        """Initializes the curve.

        Raises:
            ValueError: If `times` is empty, not strictly increasing or not
                the same length as `zero_rates`.
        """
        self._interpolator = interpolation.LinearInterpolator(times, zero_rates)

    @classmethod
    def flat(cls, zero_rate: float) -> "ZeroCurve":
        """Returns a curve with the same rate at every maturity."""
        return cls([1.0], [zero_rate])

    def zero_rate(self, t: float) -> float:
        """Returns the continuously compounded zero rate for maturity `t`."""
        return self._interpolator(t)

    def discount_factor(self, t: float) -> float:
        """Returns the present value of 1 paid at time `t`."""
        return math.exp(-self.zero_rate(t) * t)

    def forward_rate(self, t1: float, t2: float) -> float:
        """Returns the continuously compounded forward rate for [t1, t2]."""
        return (self.zero_rate(t2) * t2 - self.zero_rate(t1) * t1) / (t2 - t1)

    @property
    def times(self) -> list[float]:
        """Pillar times."""
        return self._interpolator.xs

    @property
    def zero_rates(self) -> list[float]:
        """Zero rates at the pillars."""
        return self._interpolator.ys
