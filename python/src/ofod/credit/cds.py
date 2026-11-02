"""Credit default swaps: valuation, bootstrapping, options (Ch 25.1-25.8)."""

from collections.abc import Sequence
import dataclasses
import math

from ofod.curves import credit_curve as cc
from ofod.curves import zero_curve as zc
from ofod.math import normal
from ofod.math import roots


@dataclasses.dataclass(frozen=True)
class CdsTerms:
    """Maturity in years, payments a year and recovery rate."""

    maturity: float = 5.0
    frequency: int = 4
    recovery: float = 0.4


@dataclasses.dataclass(frozen=True)
class CdsLegs:
    """PV of 1 a year of premium (with accrual) and of the payoff leg."""

    premium_annuity: float
    protection: float


def cds_leg_values(
    terms: CdsTerms, credit: cc.CreditCurve, discount: zc.ZeroCurve
) -> CdsLegs:
    """Both legs with defaults at mid-period and half-period accrual.

    Raises:
        ValueError: If maturity <= 0 or frequency < 1.
    """
    if not terms.maturity > 0.0 or terms.frequency < 1:
        raise ValueError("need maturity > 0 and frequency >= 1")
    dt = 1.0 / terms.frequency
    periods = math.ceil(terms.maturity * terms.frequency - 1e-9)
    annuity = protection = start = 0.0
    for i in range(1, periods + 1):
        end = min(i * dt, terms.maturity)
        length = end - start
        default_prob = credit.default_probability(start, end)
        df_mid = discount.discount_factor(0.5 * (start + end))
        annuity += (
            length
            * credit.survival_probability(end)
            * discount.discount_factor(end)
            + 0.5 * length * default_prob * df_mid
        )
        protection += (1.0 - terms.recovery) * default_prob * df_mid
        start = end
    return CdsLegs(annuity, protection)


def cds_par_spread(
    terms: CdsTerms, credit: cc.CreditCurve, discount: zc.ZeroCurve
) -> float:
    """Spread at which both legs are equal."""
    legs = cds_leg_values(terms, credit, discount)
    return legs.protection / legs.premium_annuity


def cds_buyer_value(
    terms: CdsTerms,
    spread: float,
    credit: cc.CreditCurve,
    discount: zc.ZeroCurve,
) -> float:
    """Value per unit notional to the protection buyer paying `spread`."""
    legs = cds_leg_values(terms, credit, discount)
    return legs.protection - spread * legs.premium_annuity


def cds_upfront(
    terms: CdsTerms,
    coupon: float,
    credit: cc.CreditCurve,
    discount: zc.ZeroCurve,
) -> float:
    """Upfront paid by the buyer on a fixed-coupon CDS (Ch 25.4)."""
    return cds_buyer_value(terms, coupon, credit, discount)


def bootstrap_credit_curve(
    maturities: Sequence[float],
    spreads: Sequence[float],
    frequency: int,
    recovery: float,
    discount: zc.ZeroCurve,
) -> cc.CreditCurve:
    """Piecewise hazards that reprice each par spread in turn.

    Raises:
        ValueError: For mismatched or non-increasing inputs, or no root.
    """
    if not maturities or len(maturities) != len(spreads):
        raise ValueError("need matching non-empty maturities and spreads")
    times: list[float] = []
    hazards: list[float] = []
    for k, m in enumerate(maturities):
        if m <= 0.0 or (k > 0 and m <= maturities[k - 1]):
            raise ValueError("maturities must be positive and increasing")
        times.append(m)
        hazards.append(0.0)
        terms = CdsTerms(m, frequency, recovery)
        target = spreads[k]

        def gap(
            h: float, terms: CdsTerms = terms, target: float = target
        ) -> float:
            hazards[-1] = h
            curve = cc.CreditCurve(times, hazards)
            return cds_par_spread(terms, curve, discount) - target

        h = roots.brent(gap, 0.0, 10.0, tolerance=1e-14)
        if h is None:
            raise ValueError(f"no hazard reprices spread {target}")
        hazards[-1] = h
    return cc.CreditCurve(times, hazards)


def cds_option_price(
    payer: bool,
    forward_spread: float,
    strike: float,
    volatility: float,
    expiry: float,
    forward_annuity: float,
) -> float:
    """Black's model for a CDS option (eq. 25.4)."""
    s = volatility * math.sqrt(expiry)
    d1 = (math.log(forward_spread / strike) + 0.5 * s * s) / s
    d2 = d1 - s
    n = normal.norm_cdf
    if payer:
        value = forward_spread * n(d1) - strike * n(d2)
    else:
        value = strike * n(-d2) - forward_spread * n(-d1)
    return forward_annuity * value
