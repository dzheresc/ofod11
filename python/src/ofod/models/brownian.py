"""Wiener processes and geometric Brownian motion (Hull, Ch 14-15).

Paths are built from explicit standard normal shocks, one per step.
"""

from collections.abc import Sequence
import dataclasses
import math


@dataclasses.dataclass(frozen=True)
class Interval:
    """A closed interval."""

    lower: float
    upper: float


@dataclasses.dataclass(frozen=True)
class ArithmeticBrownianMotion:
    """dx = a dt + b dz (generalized Wiener process, Ch 14.2)."""

    drift: float = 0.0
    volatility: float = 1.0

    def path(
        self, x0: float, dt: float, shocks: Sequence[float]
    ) -> list[float]:
        """Returns x0 then x += a dt + b sqrt(dt) eps for each shock."""
        root = math.sqrt(dt)
        out = [x0]
        for eps in shocks:
            out.append(out[-1] + self.drift * dt + self.volatility * root * eps)
        return out


@dataclasses.dataclass(frozen=True)
class GeometricBrownianMotion:
    """dS = mu S dt + sigma S dz (Ch 14.3)."""

    drift: float = 0.0
    volatility: float = 0.0

    def euler_path(
        self, s0: float, dt: float, shocks: Sequence[float]
    ) -> list[float]:
        """Discretised as in Table 14.1: dS = mu S dt + sigma S sqrt(dt) eps."""
        root = math.sqrt(dt)
        out = [s0]
        for eps in shocks:
            s = out[-1]
            out.append(
                s + self.drift * s * dt + self.volatility * s * root * eps
            )
        return out

    def exact_path(
        self, s0: float, dt: float, shocks: Sequence[float]
    ) -> list[float]:
        """S *= exp((mu - sigma^2/2) dt + sigma sqrt(dt) eps) (eq. 21.15)."""
        mu = (self.drift - 0.5 * self.volatility**2) * dt
        jump = self.volatility * math.sqrt(dt)
        out = [s0]
        for eps in shocks:
            out.append(out[-1] * math.exp(mu + jump * eps))
        return out

    def expected_value(self, s0: float, t: float) -> float:
        """Returns E(S_T) = S0 e^{mu T} (eq. 15.4)."""
        return s0 * math.exp(self.drift * t)

    def variance(self, s0: float, t: float) -> float:
        """Returns var(S_T) (eq. 15.5)."""
        return (
            s0
            * s0
            * math.exp(2.0 * self.drift * t)
            * (math.exp(self.volatility**2 * t) - 1.0)
        )

    def log_mean(self, s0: float, t: float) -> float:
        """Returns the mean of ln S_T (eq. 15.3)."""
        return math.log(s0) + (self.drift - 0.5 * self.volatility**2) * t

    def log_std_dev(self, t: float) -> float:
        """Returns the standard deviation of ln S_T."""
        return self.volatility * math.sqrt(t)

    def confidence_interval(self, s0: float, t: float, z: float) -> Interval:
        """Returns the range of S_T within `z` std devs of ln S_T."""
        m = self.log_mean(s0, t)
        s = self.log_std_dev(t)
        return Interval(math.exp(m - z * s), math.exp(m + z * s))


def quadratic_variation(path: Sequence[float]) -> float:
    """Returns the sum of squared increments (tends to T for a Wiener path)."""
    return sum((b - a) ** 2 for a, b in zip(path, path[1:], strict=False))
