"""Interest rate and currency swaps (Hull, Ch 7)."""

from collections.abc import Sequence
import dataclasses


@dataclasses.dataclass(frozen=True)
class InterestRateSwap:
    """A plain vanilla fixed-for-floating swap.

    Attributes:
        notional: Principal.
        fixed_rate: Annual fixed rate.
        frequency: Payments per year.
        payment_times: Remaining payment times in years, increasing.
        next_floating_rate: Floating rate already set for the next payment.
    """

    notional: float = 0.0
    fixed_rate: float = 0.0
    frequency: int = 2
    payment_times: Sequence[float] = ()
    next_floating_rate: float = 0.0


@dataclasses.dataclass(frozen=True)
class CurrencySwap:
    """A fixed-for-fixed currency swap exchanging principal at maturity."""

    domestic_notional: float = 0.0
    domestic_rate: float = 0.0
    foreign_notional: float = 0.0
    foreign_rate: float = 0.0
    frequency: int = 1
    payment_times: Sequence[float] = ()
