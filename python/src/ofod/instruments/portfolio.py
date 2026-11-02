"""A static portfolio of payoffs held to a common expiry (Hull, Ch 12)."""

from collections.abc import Callable
import dataclasses


@dataclasses.dataclass(frozen=True)
class Leg:
    """`quantity` units (negative for short) of `payoff` at `premium`."""

    quantity: float
    payoff: Callable[[float], float]
    premium: float = 0.0


class Portfolio:
    """A list of legs with payoff and profit at expiry."""

    def __init__(self) -> None:
        """Creates an empty portfolio."""
        self._legs: list[Leg] = []

    def add(
        self,
        quantity: float,
        payoff: Callable[[float], float],
        premium: float = 0.0,
    ) -> "Portfolio":
        """Adds a leg and returns self so calls can be chained."""
        self._legs.append(Leg(quantity, payoff, premium))
        return self

    def payoff_at(self, spot: float) -> float:
        """Returns the cash received at expiry."""
        return sum(leg.quantity * leg.payoff(spot) for leg in self._legs)

    def cost(self) -> float:
        """Returns the net set-up cost (negative for a net credit)."""
        return sum(leg.quantity * leg.premium for leg in self._legs)

    def profit_at(self, spot: float) -> float:
        """Returns payoff less cost, ignoring the time value of money."""
        return self.payoff_at(spot) - self.cost()

    @property
    def legs(self) -> list[Leg]:
        """Returns a copy of the legs."""
        return list(self._legs)
