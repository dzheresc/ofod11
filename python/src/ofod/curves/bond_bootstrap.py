"""Zero curve bootstrapping from bond prices (Hull, Ch 4.7)."""

from collections.abc import Iterable
import dataclasses

from ofod.curves import zero_curve
from ofod.instruments import bond as bond_lib
from ofod.math import roots
from ofod.pricers import bond_pricer


@dataclasses.dataclass(frozen=True)
class BondQuote:
    """A bond and its market price."""

    bond: bond_lib.Bond
    price: float


def bootstrap_from_bonds(
    quotes: Iterable[BondQuote],
) -> zero_curve.ZeroCurve:
    """Builds a zero curve with one pillar per bond maturity.

    Bonds are processed in maturity order. Coupons between known pillars use
    linear interpolation; those between the last known pillar and the new
    maturity interpolate towards the unknown rate, which is solved so the bond
    reprices exactly.

    Raises:
        ValueError: If two bonds share a maturity or a bond cannot be
            repriced.
    """
    times: list[float] = []
    rates: list[float] = []
    for quote in sorted(quotes, key=lambda q: q.bond.maturity):
        maturity = quote.bond.maturity
        if times and maturity <= times[-1]:
            raise ValueError("bond maturities must be distinct")

        def reprice_error(rate: float, quote: BondQuote = quote) -> float:
            trial = zero_curve.ZeroCurve(
                [*times, quote.bond.maturity], [*rates, rate]
            )
            return bond_pricer.bond_price(quote.bond, trial) - quote.price

        rate = roots.brent(reprice_error, -0.5, 2.0)
        if rate is None:
            raise ValueError(f"cannot reprice bond maturing at {maturity}")
        times.append(maturity)
        rates.append(rate)
    return zero_curve.ZeroCurve(times, rates)
