"""Treasury bond futures (Hull, Ch 6.1-6.2)."""

from collections.abc import Sequence
import dataclasses
import datetime
import math
import re

_THIRTY_SECONDS = re.compile(r"(\d+)-(\d+)")
_HALF_YEAR_RATE = 0.03


def parse_thirty_seconds(quote: str) -> float:
    """Parses a quote such as "95-16" (95 and 16/32) into a decimal.

    Raises:
        ValueError: If `quote` is not of the form whole-ticks with
            0 <= ticks < 32.
    """
    m = _THIRTY_SECONDS.fullmatch(quote)
    if m is None or int(m.group(2)) >= 32:
        raise ValueError(f"not a 32nds quote: {quote!r}")
    return int(m.group(1)) + int(m.group(2)) / 32.0


def accrued_interest(
    last_coupon: datetime.date,
    next_coupon: datetime.date,
    settlement: datetime.date,
    coupon_amount: float,
) -> float:
    """Returns accrued interest on an actual/actual (in period) basis."""
    elapsed = (settlement - last_coupon).days
    period = (next_coupon - last_coupon).days
    return coupon_amount * elapsed / period


def cash_price(quoted_price: float, accrued_interest_: float) -> float:
    """Returns the dirty (cash) price: quoted price plus accrued interest."""
    return quoted_price + accrued_interest_


def _value_with(coupon: float, n: int) -> float:
    v = 100.0 * math.pow(1.0 + _HALF_YEAR_RATE, -n)
    for k in range(1, n + 1):
        v += coupon * math.pow(1.0 + _HALF_YEAR_RATE, -k)
    return v


def conversion_factor(coupon_rate: float, years_to_maturity: float) -> float:
    """Returns the CBOT conversion factor of a deliverable bond.

    The maturity is rounded down to whole quarters and the bond is valued
    per dollar of principal at 6% per annum with semiannual compounding.
    """
    coupon = 100.0 * coupon_rate / 2.0
    quarters = math.floor(years_to_maturity * 4.0)
    half_years = quarters // 2
    if quarters % 2 == 0:
        return _value_with(coupon, half_years) / 100.0
    # Odd quarter: value three months ahead (a coupon is then due), discount
    # back three months and subtract three months of accrued interest.
    ahead = coupon + _value_with(coupon, half_years)
    back = ahead / math.sqrt(1.0 + _HALF_YEAR_RATE)
    return (back - coupon / 2.0) / 100.0


@dataclasses.dataclass(frozen=True)
class DeliverableBond:
    """A bond in the delivery basket."""

    quoted_price: float
    conversion_factor: float


@dataclasses.dataclass(frozen=True)
class CheapestToDeliver:
    """The cheapest bond and its delivery cost.

    Attributes:
        index: Position of the cheapest bond in the input.
        cost: Quoted price - futures price * conversion factor.
    """

    index: int
    cost: float


def find_cheapest_to_deliver(
    futures_price: float, bonds: Sequence[DeliverableBond]
) -> CheapestToDeliver:
    """Returns the bond that minimises quoted price - F * CF.

    Raises:
        ValueError: If `bonds` is empty.
    """
    if not bonds:
        raise ValueError("no deliverable bonds")
    costs = [
        b.quoted_price - futures_price * b.conversion_factor for b in bonds
    ]
    index = min(range(len(costs)), key=costs.__getitem__)
    return CheapestToDeliver(index, costs[index])


def treasury_futures_quote(
    cash_bond_price: float,
    coupons_pv: float,
    rate: float,
    t: float,
    accrued_at_delivery: float,
    conversion_factor_: float,
) -> float:
    """Returns the quoted futures price assuming the CTD and delivery date.

    The cash futures price (S - I) e^{rT} is converted to a quote by
    removing accrued interest at delivery and dividing by the CTD's
    conversion factor.
    """
    cash_futures = (cash_bond_price - coupons_pv) * math.exp(rate * t)
    return (cash_futures - accrued_at_delivery) / conversion_factor_
