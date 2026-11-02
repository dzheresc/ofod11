"""Employee stock options (Hull, Ch 16)."""

import dataclasses
import math

from ofod.instruments import option as option_lib
from ofod.pricers import binomial_tree
from ofod.pricers import black_scholes


@dataclasses.dataclass(frozen=True)
class EsoTerms:
    """Hull-White ESO terms; exercise once S >= exercise_multiple * K."""

    strike: float = 0.0
    maturity: float = 0.0
    vesting: float = 0.0
    exercise_multiple: float = 0.0
    exit_rate: float = 0.0


def price_employee_stock_option(
    terms: EsoTerms, market: binomial_tree.TreeMarket, steps: int
) -> float:
    """Hull-White binomial ESO value (Ch 16.3).

    After vesting the option is exercised once S >= M K; otherwise, with
    probability 1 - e^{-exit_rate dt} per step, the employee leaves and
    exercises if in the money. Before vesting a leaver forfeits the option.

    Raises:
        ValueError: For steps < 1, M < 1, a negative exit rate or vesting
            outside [0, T].
    """
    if (
        steps < 1
        or terms.exercise_multiple < 1.0
        or terms.exit_rate < 0.0
        or not 0.0 <= terms.vesting <= terms.maturity
    ):
        raise ValueError("invalid employee stock option terms")
    dt = terms.maturity / steps
    step = binomial_tree.scheme_step(
        binomial_tree.TreeScheme.COX_ROSS_RUBINSTEIN,
        market.volatility,
        market.rate - market.yield_,
        dt,
    )
    p = step.probability
    if not 0.0 <= p <= 1.0:
        raise ValueError("invalid tree: p outside [0, 1]")
    disc = math.exp(-market.rate * dt)
    leave = 1.0 - math.exp(-terms.exit_rate * dt)
    k = terms.strike
    trigger = terms.exercise_multiple * k
    first_vested = math.ceil(terms.vesting / dt - 1e-9)

    def spot(i: int, j: int) -> float:
        return market.spot * math.pow(step.up, j) * math.pow(step.down, i - j)

    values = [max(spot(steps, j) - k, 0.0) for j in range(steps + 1)]
    for i in range(steps - 1, -1, -1):
        vested = i >= first_vested
        for j in range(i + 1):
            s = spot(i, j)
            hold = disc * (p * values[j + 1] + (1.0 - p) * values[j])
            if not vested:
                v = (1.0 - leave) * hold
            elif s >= trigger:
                v = s - k
            else:
                v = (1.0 - leave) * hold + leave * max(s - k, 0.0)
            values[j] = v
    return values[0]


def expected_life_value(
    spot: float,
    strike: float,
    rate: float,
    yield_: float,
    volatility: float,
    expected_life: float,
) -> float:
    """BSM call with the expected life as maturity (Ch 16.4)."""
    return black_scholes.bsm_price(
        option_lib.OptionType.CALL,
        spot,
        strike,
        rate,
        yield_,
        volatility,
        expected_life,
    )
