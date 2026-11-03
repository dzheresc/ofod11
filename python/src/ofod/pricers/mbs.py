"""Agency mortgage-backed securities (Hull, Ch 33.4).

The PSA prepayment benchmark, pass-through cash flows split into IO and
PO strips, and Monte Carlo with rate-dependent prepayments under
Hull-White.
"""

import dataclasses
import math

import numpy as np

from ofod.curves import zero_curve
from ofod.models import short_rate

_MONTH = 1.0 / 12.0


@dataclasses.dataclass(frozen=True)
class MortgagePool:
    """Level-payment mortgages paying mortgage_rate over `months`."""

    balance: float
    mortgage_rate: float
    pass_through_rate: float
    months: int
    age: int = 0


@dataclasses.dataclass(frozen=True)
class MbsCashFlow:
    """One month of investor cash flows."""

    month: int
    interest: float
    scheduled: float
    prepaid: float
    balance: float


@dataclasses.dataclass(frozen=True)
class MbsValue:
    """Pass-through, interest-only and principal-only values."""

    pass_through: float
    interest_only: float
    principal_only: float


@dataclasses.dataclass(frozen=True)
class RefinancingModel:
    """CPR = base + sensitivity x max(WAC - (r + spread), 0), capped."""

    base_cpr: float = 0.06
    sensitivity: float = 5.0
    spread: float = 0.015
    max_cpr: float = 0.6


@dataclasses.dataclass(frozen=True)
class MbsMcOptions:
    """Paths and seed."""

    paths: int = 5_000
    seed: int = 1


def psa_cpr(month: int, speed: float) -> float:
    """Returns 6% x speed / 100 x min(month / 30, 1)."""
    return 0.06 * speed / 100.0 * min(month / 30.0, 1.0)


def monthly_mortality(cpr: float) -> float:
    """Returns 1 - (1 - CPR)^(1/12)."""
    return 1.0 - math.pow(1.0 - cpr, _MONTH)


def _level_payment(balance: float, i: float, n: int) -> float:
    if i == 0.0:
        return balance / n
    return balance * i / -math.expm1(-n * math.log1p(i))


def pass_through_cash_flows(
    pool: MortgagePool, psa_speed: float
) -> list[MbsCashFlow]:
    """Returns monthly cash flows at a constant PSA speed."""
    flows = []
    balance = pool.balance
    i_mortgage = pool.mortgage_rate / 12.0
    for m in range(1, pool.months + 1):
        if balance <= 1e-12:
            break
        smm = monthly_mortality(psa_cpr(pool.age + m, psa_speed))
        payment = _level_payment(balance, i_mortgage, pool.months - m + 1)
        scheduled = min(payment - balance * i_mortgage, balance)
        prepaid = smm * (balance - scheduled)
        interest = balance * pool.pass_through_rate / 12.0
        balance -= scheduled + prepaid
        flows.append(MbsCashFlow(m, interest, scheduled, prepaid, balance))
    return flows


def value_cash_flows(
    flows: list[MbsCashFlow], curve: zero_curve.ZeroCurve
) -> MbsValue:
    """Discounts month m at curve.discount_factor(m / 12)."""
    io = 0.0
    po = 0.0
    for f in flows:
        df = curve.discount_factor(f.month * _MONTH)
        io += df * f.interest
        po += df * (f.scheduled + f.prepaid)
    return MbsValue(io + po, io, po)


def mbs_monte_carlo(
    pool: MortgagePool,
    rates: short_rate.HullWhite,
    model: RefinancingModel | None = None,
    options: MbsMcOptions | None = None,
) -> MbsValue:
    """Values the pool with exact monthly Hull-White short rates.

    Raises:
        ValueError: For no paths or months.
    """
    model = model or RefinancingModel()
    options = options or MbsMcOptions()
    if options.paths <= 0 or pool.months <= 0:
        raise ValueError("need paths and months")
    a, sigma = rates.a, rates.sigma
    decay = math.exp(-a * _MONTH) if a > 0.0 else 1.0
    var = -math.expm1(-2.0 * a * _MONTH) / (2.0 * a) if a > 0.0 else _MONTH
    x_sd = sigma * math.sqrt(var)
    phi = [
        rates.instantaneous_forward(m * _MONTH)
        + 0.5 * sigma**2 * rates.b(0.0, m * _MONTH) ** 2
        for m in range(pool.months + 1)
    ]
    rng = np.random.default_rng(options.seed)
    n = options.paths
    x = np.zeros(n)
    r = np.full(n, phi[0])
    log_df = np.zeros(n)
    balance = np.full(n, pool.balance)
    io = np.zeros(n)
    po = np.zeros(n)
    i_mortgage = pool.mortgage_rate / 12.0
    for m in range(1, pool.months + 1):
        r_start = r
        x = x * decay + x_sd * rng.standard_normal(n)
        r = x + phi[m]
        log_df -= 0.5 * (r_start + r) * _MONTH
        incentive = np.maximum(pool.mortgage_rate - (r_start + model.spread), 0)
        seasoning = min((pool.age + m) / 30.0, 1.0)
        cpr = np.minimum(
            seasoning * (model.base_cpr + model.sensitivity * incentive),
            model.max_cpr,
        )
        smm = 1.0 - (1.0 - cpr) ** _MONTH
        remaining = pool.months - m + 1
        if i_mortgage == 0.0:
            payment = balance / remaining
        else:
            payment = (
                balance
                * i_mortgage
                / -math.expm1(-remaining * math.log1p(i_mortgage))
            )
        scheduled = np.minimum(payment - balance * i_mortgage, balance)
        prepaid = smm * (balance - scheduled)
        df = np.exp(log_df)
        io += df * balance * pool.pass_through_rate / 12.0
        po += df * (scheduled + prepaid)
        balance = balance - scheduled - prepaid
    io_value = float(io.mean())
    po_value = float(po.mean())
    return MbsValue(io_value + po_value, io_value, po_value)
