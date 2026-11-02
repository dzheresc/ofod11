"""Hedging strategies using futures (Hull, Ch 3)."""

from collections.abc import Iterable
from collections.abc import Sequence
import dataclasses

from ofod.stats import descriptive


def minimum_variance_hedge_ratio(
    correlation: float, spot_vol: float, futures_vol: float
) -> float:
    """Returns h* = rho * sigma_S / sigma_F (eq. 3.1)."""
    return correlation * spot_vol / futures_vol


def hedge_ratio_from_changes(
    spot_changes: Sequence[float], futures_changes: Sequence[float]
) -> float:
    """Returns h* as the slope of spot changes regressed on futures changes.

    Raises:
        ValueError: If the samples are invalid or futures changes constant.
    """
    return descriptive.linear_regression(futures_changes, spot_changes).slope


def hedge_effectiveness(correlation: float) -> float:
    """Returns the hedge effectiveness rho^2."""
    return correlation * correlation


def optimal_contracts(
    hedge_ratio: float, exposure_units: float, contract_units: float
) -> float:
    """Returns N* = h* Q_A / Q_F (eq. 3.2)."""
    return hedge_ratio * exposure_units / contract_units


def tailed_contracts(
    hedge_ratio: float, exposure_value: float, futures_value: float
) -> float:
    """Returns N* = h* V_A / V_F with tailing (eq. 3.3)."""
    return hedge_ratio * exposure_value / futures_value


def beta_hedge_contracts(
    beta: float, portfolio_value: float, futures_value: float
) -> float:
    """Returns N* = beta V_A / V_F for hedging an equity portfolio."""
    return beta * portfolio_value / futures_value


def change_beta_contracts(
    beta: float,
    target_beta: float,
    portfolio_value: float,
    futures_value: float,
) -> float:
    """Returns (beta* - beta) V_A / V_F; negative means short futures."""
    return (target_beta - beta) * portfolio_value / futures_value


def basis(spot: float, futures: float) -> float:
    """Returns the basis S - F."""
    return spot - futures


def effective_hedge_price(
    spot_at_close: float, futures_at_open: float, futures_at_close: float
) -> float:
    """Returns S2 + F1 - F2, the price realised with a hedge."""
    return spot_at_close + futures_at_open - futures_at_close


@dataclasses.dataclass(frozen=True)
class RollLeg:
    """One futures contract in a stack-and-roll hedge."""

    open_price: float
    close_price: float


def short_roll_gain(legs: Iterable[RollLeg]) -> float:
    """Returns the gain per unit from rolling short futures positions."""
    return sum(leg.open_price - leg.close_price for leg in legs)
