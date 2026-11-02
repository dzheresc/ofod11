"""Forward prices and forward contract values (Hull, Ch 5).

All rates are continuously compounded; times are in years.
"""

from collections.abc import Iterable
import enum
import math

from ofod.curves import zero_curve
from ofod.instruments import cash_flow
from ofod.instruments import payoff


def forward_price(spot: float, rate: float, t: float) -> float:
    """Returns F0 = S0 exp(rT) for an asset with no income (eq. 5.1)."""
    return spot * math.exp(rate * t)


def forward_price_with_income(
    spot: float, income_pv: float, rate: float, t: float
) -> float:
    """Returns F0 = (S0 - I) exp(rT) (eq. 5.2)."""
    return (spot - income_pv) * math.exp(rate * t)


def forward_price_with_yield(
    spot: float, rate: float, yield_: float, t: float
) -> float:
    """Returns F0 = S0 exp((r - q) T) (eq. 5.3).

    Also covers stock indices (q = dividend yield) and currencies
    (q = foreign risk-free rate, eq. 5.9).
    """
    return spot * math.exp((rate - yield_) * t)


def forward_price_cost_of_carry(
    spot: float,
    rate: float,
    storage_yield: float,
    convenience_yield: float,
    t: float,
) -> float:
    """Returns F0 = S0 exp((r + u - y) T) (Ch 5.11-5.12)."""
    return spot * math.exp((rate + storage_yield - convenience_yield) * t)


def present_value(
    flows: Iterable[cash_flow.CashFlow], curve: zero_curve.ZeroCurve
) -> float:
    """Returns the present value of `flows` discounted on `curve`."""
    return sum(f.amount * curve.discount_factor(f.time) for f in flows)


def forward_contract_value(
    forward_price_: float,
    delivery_price: float,
    rate: float,
    t: float,
    side: payoff.Side,
) -> float:
    """Returns (F0 - K) exp(-rT) for a long, the negative for a short."""
    return (
        payoff.sign(side)
        * (forward_price_ - delivery_price)
        * math.exp(-rate * t)
    )


class ForwardArbitrage(enum.Enum):
    """Which arbitrage a mispriced forward allows."""

    NONE = "none"
    CASH_AND_CARRY = "cash_and_carry"  # Forward too rich.
    REVERSE_CASH_AND_CARRY = "reverse_cash_and_carry"  # Forward too cheap.


def detect_forward_arbitrage(
    quoted_forward: float, theoretical: float, tolerance: float = 1e-9
) -> ForwardArbitrage:
    """Classifies a quoted forward against its theoretical price."""
    if quoted_forward > theoretical + tolerance:
        return ForwardArbitrage.CASH_AND_CARRY
    if quoted_forward < theoretical - tolerance:
        return ForwardArbitrage.REVERSE_CASH_AND_CARRY
    return ForwardArbitrage.NONE
