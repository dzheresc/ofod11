"""Default probabilities with piecewise-constant hazards (Ch 24.1-24.3)."""

from collections.abc import Sequence
import math


class CreditCurve:
    """hazards[i] applies on (times[i-1], times[i]]; the last one beyond."""

    def __init__(
        self, times: Sequence[float], hazards: Sequence[float]
    ) -> None:
        """Initializes the curve.

        Raises:
            ValueError: Unless times are non-empty, positive and strictly
                increasing, sizes match and no hazard is negative.
        """
        if not times or len(times) != len(hazards) or times[0] <= 0.0:
            raise ValueError("invalid credit curve")
        for i in range(len(times)):
            if hazards[i] < 0.0 or (i > 0 and times[i] <= times[i - 1]):
                raise ValueError("invalid credit curve")
        self._times = [float(t) for t in times]
        self._hazards = [float(h) for h in hazards]

    @classmethod
    def flat(cls, hazard: float) -> "CreditCurve":
        """The same hazard rate at every horizon."""
        return cls([1.0], [hazard])

    @property
    def times(self) -> list[float]:
        """Hazard segment end times."""
        return list(self._times)

    @property
    def hazards(self) -> list[float]:
        """Hazard rates per segment."""
        return list(self._hazards)

    def survival_probability(self, t: float) -> float:
        """Q(t) = exp(-int_0^t lambda)."""
        if t <= 0.0:
            return 1.0
        integral = 0.0
        start = 0.0
        n = len(self._times)
        for i in range(n):
            end = t if i + 1 == n else min(t, self._times[i])
            if end > start:
                integral += self._hazards[i] * (end - start)
            if t <= self._times[i]:
                break
            start = self._times[i]
        return math.exp(-integral)

    def default_probability(self, t1: float, t2: float | None = None) -> float:
        """1 - Q(t1), or Q(t1) - Q(t2) for default in (t1, t2]."""
        if t2 is None:
            return 1.0 - self.survival_probability(t1)
        return self.survival_probability(t1) - self.survival_probability(t2)

    def hazard(self, t: float) -> float:
        """Instantaneous hazard rate at t."""
        for ti, h in zip(self._times, self._hazards, strict=True):
            if t <= ti:
                return h
        return self._hazards[-1]


def hazard_from_spread(spread: float, recovery: float) -> float:
    """Returns the hazard rate s / (1 - R) (eq. 24.2)."""
    return spread / (1.0 - recovery)


def average_hazard(cumulative_default: float, t: float) -> float:
    """-ln(1 - Q(t)) / t (eq. 24.1)."""
    return -math.log(1.0 - cumulative_default) / t
