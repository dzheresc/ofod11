"""Payoffs of the basic positions (Hull, Ch 1.3-1.5)."""

import dataclasses
import enum
from typing import Protocol


class Payoff(Protocol):
    """Maps the underlying price at expiry to a cash amount."""

    def __call__(self, spot: float) -> float:
        """Returns the payoff for `spot`."""
        ...


class Side(enum.Enum):
    """Long or short."""

    LONG = "long"
    SHORT = "short"


def sign(side: Side) -> float:
    """Returns +1 for long and -1 for short."""
    return 1.0 if side is Side.LONG else -1.0


@dataclasses.dataclass(frozen=True)
class ForwardPayoff:
    """Long forward: S_T - K."""

    delivery_price: float

    def __call__(self, spot: float) -> float:
        """Returns the payoff for `spot`."""
        return spot - self.delivery_price


@dataclasses.dataclass(frozen=True)
class CallPayoff:
    """Long call: max(S_T - K, 0)."""

    strike: float

    def __call__(self, spot: float) -> float:
        """Returns the payoff for `spot`."""
        return max(spot - self.strike, 0.0)


@dataclasses.dataclass(frozen=True)
class PutPayoff:
    """Long put: max(K - S_T, 0)."""

    strike: float

    def __call__(self, spot: float) -> float:
        """Returns the payoff for `spot`."""
        return max(self.strike - spot, 0.0)


@dataclasses.dataclass(frozen=True)
class Position:
    """A holding of `quantity` units of a payoff.

    Attributes:
        payoff: What one unit pays at expiry.
        side: Long or short.
        quantity: Number of units.
        premium: Price per unit paid (long) or received (short) at inception.
    """

    payoff: Payoff
    side: Side = Side.LONG
    quantity: float = 1.0
    premium: float = 0.0

    def payoff_at(self, spot: float) -> float:
        """Returns the cash received at expiry."""
        return sign(self.side) * self.quantity * self.payoff(spot)

    def profit_at(self, spot: float) -> float:
        """Returns the payoff net of the premium, ignoring discounting."""
        return (
            sign(self.side) * self.quantity * (self.payoff(spot) - self.premium)
        )
