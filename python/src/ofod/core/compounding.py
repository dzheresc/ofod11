"""Interest rate compounding conventions (Hull, Ch 4.4)."""

import dataclasses
import enum
import math


class CompoundingKind(enum.Enum):
    """How interest accrues."""

    CONTINUOUS = "continuous"
    PERIODIC = "periodic"
    SIMPLE = "simple"


@dataclasses.dataclass(frozen=True)
class Compounding:
    """A compounding convention.

    Attributes:
        kind: Continuous, periodic or simple compounding.
        frequency: Compounding periods per year; used only when periodic.
    """

    kind: CompoundingKind
    frequency: int = 0

    @classmethod
    def continuous(cls) -> "Compounding":
        """Returns continuous compounding: growth exp(R t)."""
        return cls(CompoundingKind.CONTINUOUS)

    @classmethod
    def periodic(cls, frequency: int) -> "Compounding":
        """Returns compounding m times a year: growth (1 + R/m)^(m t)."""
        return cls(CompoundingKind.PERIODIC, frequency)

    @classmethod
    def simple(cls) -> "Compounding":
        """Returns simple interest: growth 1 + R t."""
        return cls(CompoundingKind.SIMPLE)


def growth_factor(rate: float, t: float, c: Compounding) -> float:
    """Returns the amount that 1 grows to over `t` years at `rate`."""
    if c.kind is CompoundingKind.CONTINUOUS:
        return math.exp(rate * t)
    if c.kind is CompoundingKind.PERIODIC:
        m = c.frequency
        return math.pow(1.0 + rate / m, m * t)
    return 1.0 + rate * t


def discount_factor(rate: float, t: float, c: Compounding) -> float:
    """Returns the present value of 1 paid in `t` years at `rate`."""
    return 1.0 / growth_factor(rate, t, c)


def implied_rate(growth: float, t: float, c: Compounding) -> float:
    """Returns the rate under `c` that grows 1 to `growth` over `t` years."""
    if c.kind is CompoundingKind.CONTINUOUS:
        return math.log(growth) / t
    if c.kind is CompoundingKind.PERIODIC:
        m = c.frequency
        return m * (math.pow(growth, 1.0 / (m * t)) - 1.0)
    return (growth - 1.0) / t


def equivalent_rate(
    rate: float, from_c: Compounding, to_c: Compounding, t: float = 1.0
) -> float:
    """Converts `rate` under `from_c` to the equivalent rate under `to_c`.

    Args:
        rate: The rate to convert.
        from_c: Convention `rate` is expressed in.
        to_c: Target convention.
        t: Horizon in years; matters only for simple compounding.

    Returns:
        The rate under `to_c` with the same growth over `t` years.
    """
    return implied_rate(growth_factor(rate, t, from_c), t, to_c)
