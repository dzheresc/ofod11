"""Zero curve from par swap rates (Hull, Ch 7.6)."""

from collections.abc import Iterable
import dataclasses

from ofod.curves import bond_bootstrap
from ofod.curves import zero_curve
from ofod.instruments import bond


@dataclasses.dataclass(frozen=True)
class SwapQuote:
    """A par swap rate.

    Attributes:
        maturity: Years; payments every 1/frequency years.
        par_rate: Fixed rate giving the swap zero value.
        frequency: Payments per year.
    """

    maturity: float = 0.0
    par_rate: float = 0.0
    frequency: int = 1


def bootstrap_from_swaps(quotes: Iterable[SwapQuote]) -> zero_curve.ZeroCurve:
    """Bootstraps a zero curve treating each swap as a par bond.

    Raises:
        ValueError: If maturities repeat or a bond cannot be repriced.
    """
    return bond_bootstrap.bootstrap_from_bonds(
        bond_bootstrap.BondQuote(
            bond.Bond(
                face=100.0,
                coupon_rate=q.par_rate,
                maturity=q.maturity,
                frequency=q.frequency,
            ),
            100.0,
        )
        for q in quotes
    )
