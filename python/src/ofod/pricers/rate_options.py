"""Standard market models for interest rate options (Hull, Ch 29).

Black's model for bond options, caps, floors and swaptions, the normal
(Bachelier) model, caplet volatility stripping and curve-bump hedging.
Zero curves are continuously compounded; times are in years.
"""

from collections.abc import Callable
from collections.abc import Sequence
import dataclasses
import math

from ofod.curves import zero_curve
from ofod.instruments.option import OptionType
from ofod.math import normal
from ofod.math import roots
from ofod.pricers.fra_pricer import simple_forward_rate

ZeroCurve = zero_curve.ZeroCurve
CurvePricer = Callable[[ZeroCurve], float]


def _sign(option_type: OptionType) -> float:
    return 1.0 if option_type is OptionType.CALL else -1.0


def forward_bond_price(
    spot_price: float, coupon_pv: float, discount: float
) -> float:
    """Returns (B0 - I) / P(0, T) (eq. 29.1)."""
    return (spot_price - coupon_pv) / discount


def black_formula(
    option_type: OptionType,
    forward: float,
    strike: float,
    volatility: float,
    t: float,
) -> float:
    """Returns the undiscounted Black-76 value."""
    phi = _sign(option_type)
    sd = volatility * math.sqrt(t)
    if sd <= 0.0:
        return max(phi * (forward - strike), 0.0)
    d1 = math.log(forward / strike) / sd + 0.5 * sd
    d2 = d1 - sd
    return phi * (
        forward * normal.norm_cdf(phi * d1) - strike * normal.norm_cdf(phi * d2)
    )


def bachelier_formula(
    option_type: OptionType,
    forward: float,
    strike: float,
    normal_vol: float,
    t: float,
) -> float:
    """Returns the undiscounted normal-model value (Ch 29.4)."""
    phi = _sign(option_type)
    sd = normal_vol * math.sqrt(t)
    if sd <= 0.0:
        return max(phi * (forward - strike), 0.0)
    d = (forward - strike) / sd
    return phi * (forward - strike) * normal.norm_cdf(
        phi * d
    ) + sd * normal.norm_pdf(d)


def black_bond_option_price(
    option_type: OptionType,
    forward_price: float,
    strike: float,
    discount: float,
    volatility: float,
    t: float,
) -> float:
    """Returns Black's price on the forward bond price (eq. 29.2)."""
    return discount * black_formula(
        option_type, forward_price, strike, volatility, t
    )


def price_volatility_from_yield(
    modified_duration: float, forward_yield: float, yield_volatility: float
) -> float:
    """Returns D y0 sigma_y (eq. 29.4)."""
    return modified_duration * forward_yield * yield_volatility


def black_caplet_price(
    option_type: OptionType,
    forward: float,
    strike: float,
    volatility: float,
    reset: float,
    accrual: float,
    discount: float,
    notional: float,
) -> float:
    """Returns L tau P(0, t_k+1) [F N(d1) - K N(d2)] (eqs. 29.7, 29.8)."""
    return (
        notional
        * accrual
        * discount
        * black_formula(option_type, forward, strike, volatility, reset)
    )


@dataclasses.dataclass(frozen=True)
class CapTerms:
    """Caplets reset at k tau, k = 1..n-1, and pay at (k + 1) tau."""

    strike: float
    maturity: float
    frequency: int = 4
    notional: float = 1.0


def caplet_count(terms: CapTerms) -> int:
    """Returns the number of caplets, or 0 for a bad schedule."""
    if terms.frequency <= 0 or terms.maturity <= 0.0:
        return 0
    periods = terms.maturity * terms.frequency
    n = round(periods)
    if abs(periods - n) > 1e-9 or n < 2:
        return 0
    return int(n) - 1


def caplet_prices(
    option_type: OptionType,
    curve: ZeroCurve,
    terms: CapTerms,
    volatilities: Sequence[float],
) -> list[float]:
    """Returns each caplet's Black price.

    Raises:
        ValueError: For a bad schedule or one vol per caplet not given.
    """
    n = caplet_count(terms)
    if n == 0 or len(volatilities) != n:
        raise ValueError("bad cap schedule or volatilities")
    tau = 1.0 / terms.frequency
    out = []
    for k, vol in enumerate(volatilities):
        reset = (k + 1) * tau
        pay = reset + tau
        out.append(
            black_caplet_price(
                option_type,
                simple_forward_rate(curve, reset, pay),
                terms.strike,
                vol,
                reset,
                tau,
                curve.discount_factor(pay),
                terms.notional,
            )
        )
    return out


def cap_floor_price(
    option_type: OptionType,
    curve: ZeroCurve,
    terms: CapTerms,
    flat_volatility: float,
) -> float:
    """Returns a cap (CALL) or floor (PUT) with one flat volatility.

    Raises:
        ValueError: For a bad schedule.
    """
    n = caplet_count(terms)
    if n == 0:
        raise ValueError("bad cap schedule")
    return sum(caplet_prices(option_type, curve, terms, [flat_volatility] * n))


def strip_caplet_volatilities(
    curve: ZeroCurve,
    strike: float,
    frequency: int,
    flat_vols: Sequence[float],
    notional: float = 1.0,
) -> list[float]:
    """Returns spot caplet vols from flat cap vols (Ch 29.2).

    flat_vols[i] is quoted for the cap with i + 1 caplets.

    Raises:
        ValueError: For bad inputs or a vol that cannot be solved for.
    """
    if frequency <= 0 or not flat_vols:
        raise ValueError("bad inputs")
    tau = 1.0 / frequency
    spot: list[float] = []
    previous = 0.0
    for i, flat in enumerate(flat_vols):
        terms = CapTerms(strike, (i + 2) * tau, frequency, notional)
        cap = cap_floor_price(OptionType.CALL, curve, terms, flat)
        target = cap - previous
        reset = (i + 1) * tau
        pay = reset + tau
        forward = simple_forward_rate(curve, reset, pay)
        discount = curve.discount_factor(pay)

        def error(
            vol: float,
            forward: float = forward,
            reset: float = reset,
            discount: float = discount,
            target: float = target,
        ) -> float:
            return (
                black_caplet_price(
                    OptionType.CALL,
                    forward,
                    strike,
                    vol,
                    reset,
                    tau,
                    discount,
                    notional,
                )
                - target
            )

        vol = roots.brent(error, 1e-6, 5.0)
        if vol is None:
            raise ValueError("caplet volatility not found")
        spot.append(vol)
        previous = cap
    return spot


@dataclasses.dataclass(frozen=True)
class ForwardSwap:
    """Annuity sum tau P(0, t_i) and the forward swap rate."""

    annuity: float
    rate: float


def forward_swap_rate(
    curve: ZeroCurve, start: float, tenor: float, frequency: int
) -> ForwardSwap:
    """Returns the annuity and forward rate of a forward-start swap."""
    tau = 1.0 / frequency
    n = round(tenor * frequency)
    annuity = sum(
        tau * curve.discount_factor(start + i * tau) for i in range(1, n + 1)
    )
    end = start + n * tau
    rate = (curve.discount_factor(start) - curve.discount_factor(end)) / annuity
    return ForwardSwap(annuity, rate)


def black_swaption_price(
    option_type: OptionType,
    forward_rate: float,
    strike: float,
    volatility: float,
    expiry: float,
    annuity: float,
    notional: float,
) -> float:
    """Returns L A [s0 N(d1) - K N(d2)]; CALL is a payer (eq. 29.10)."""
    return (
        notional
        * annuity
        * black_formula(option_type, forward_rate, strike, volatility, expiry)
    )


def bachelier_swaption_price(
    option_type: OptionType,
    forward_rate: float,
    strike: float,
    normal_vol: float,
    expiry: float,
    annuity: float,
    notional: float,
) -> float:
    """Returns the swaption price under the normal model."""
    return (
        notional
        * annuity
        * bachelier_formula(
            option_type, forward_rate, strike, normal_vol, expiry
        )
    )


def _bumped(curve: ZeroCurve, bumps: Sequence[float]) -> ZeroCurve:
    rates = [r + b for r, b in zip(curve.zero_rates, bumps, strict=True)]
    return ZeroCurve(curve.times, rates)


def bucket_dv01(
    value: CurvePricer, curve: ZeroCurve, bump: float = 1e-4
) -> list[float]:
    """Returns the value change for each pillar bumped in turn."""
    base = value(curve)
    n = len(curve.times)
    out = []
    for i in range(n):
        bumps = [0.0] * n
        bumps[i] = bump
        out.append(value(_bumped(curve, bumps)) - base)
    return out


def parallel_dv01(
    value: CurvePricer, curve: ZeroCurve, bump: float = 1e-4
) -> float:
    """Returns the value change for a parallel shift of all pillars."""
    return value(_bumped(curve, [bump] * len(curve.times))) - value(curve)
