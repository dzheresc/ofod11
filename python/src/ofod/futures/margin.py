"""Daily settlement of a futures margin account (Hull, Ch 2.4, Table 2.1)."""

from collections.abc import Sequence
import dataclasses

from ofod.instruments import payoff


@dataclasses.dataclass(frozen=True)
class MarginSpec:
    """A futures position and its margin requirements.

    Attributes:
        initial_margin: Per contract.
        maintenance_margin: Per contract.
        contract_size: Units of the asset per contract.
        contracts: Number of contracts.
        side: Long or short.
    """

    initial_margin: float = 0.0
    maintenance_margin: float = 0.0
    contract_size: float = 1.0
    contracts: int = 1
    side: payoff.Side = payoff.Side.LONG


@dataclasses.dataclass(frozen=True)
class MarginDay:
    """One row of the margin account table.

    Attributes:
        price: Settlement price.
        daily_gain: Gain or loss since the previous settlement.
        cumulative_gain: Gain or loss since the position was opened.
        balance: Account balance before any margin call is paid.
        margin_call: Paid to restore the initial margin; 0 if none.
    """

    price: float
    daily_gain: float
    cumulative_gain: float
    balance: float
    margin_call: float


def simulate_margin_account(
    spec: MarginSpec, prices: Sequence[float]
) -> list[MarginDay]:
    """Marks the position to market at each settlement price.

    The first price is where the position is opened. A margin call is made
    whenever the balance falls below the maintenance margin and restores the
    account to the initial margin.
    """
    days: list[MarginDay] = []
    if not prices:
        return days
    initial = spec.initial_margin * spec.contracts
    maintenance = spec.maintenance_margin * spec.contracts
    units = payoff.sign(spec.side) * spec.contract_size * spec.contracts
    days.append(MarginDay(prices[0], 0.0, 0.0, initial, 0.0))
    balance = initial
    cumulative = 0.0
    for prev, price in zip(prices, prices[1:], strict=False):
        gain = units * (price - prev)
        cumulative += gain
        balance += gain
        call = initial - balance if balance < maintenance else 0.0
        days.append(MarginDay(price, gain, cumulative, balance, call))
        balance += call
    return days
