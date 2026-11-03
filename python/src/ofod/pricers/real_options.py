"""Real options (Hull, Ch 36).

The market price of risk from CAPM, risk-neutral valuation of a business by
Monte Carlo, and the options to abandon, expand, contract or defer valued on
a binomial lattice for the project value and on the commodity tree of
Ch 35.
"""

from collections.abc import Callable
from collections.abc import Sequence
import dataclasses
import math

import numpy as np

from ofod.pricers.commodity_tree import CommodityTree


def market_price_of_risk(
    correlation: float, market_return: float, rate: float, market_vol: float
) -> float:
    """Returns lambda = rho (mu_m - r) / sigma_m (Ch 36.2)."""
    return correlation * (market_return - rate) / market_vol


def risk_neutral_growth(
    growth: float, lambda_: float, volatility: float
) -> float:
    """The risk-neutral growth rate m - lambda s."""
    return growth - lambda_ * volatility


def risk_neutral_present_value(
    times: Sequence[float],
    expected: Sequence[float],
    lambda_: float,
    volatility: float,
    rate: float,
) -> float:
    """PV of real-world expected cash flows reduced by e^{-lambda s t}.

    Raises:
        ValueError: If the sequences differ in length.
    """
    if len(times) != len(expected):
        raise ValueError("times and expected must match")
    return sum(
        e * math.exp(-(lambda_ * volatility + rate) * t)
        for t, e in zip(times, expected, strict=True)
    )


@dataclasses.dataclass(frozen=True)
class Abandon:
    """Sell the project for `salvage`."""

    salvage: float


@dataclasses.dataclass(frozen=True)
class Expand:
    """Scale by `factor` > 1 at a cost: factor V - cost."""

    factor: float
    cost: float


@dataclasses.dataclass(frozen=True)
class Contract:
    """Scale by `factor` < 1 and save: factor V + saving."""

    factor: float
    saving: float


ProjectOption = Abandon | Expand | Contract


def exercise_value(option: ProjectOption, value: float) -> float:
    """Value received on exercising `option` when the project is worth V."""
    match option:
        case Abandon(salvage=s):
            return s
        case Expand(factor=f, cost=c):
            return f * value - c
        case Contract(factor=f, saving=s):
            return f * value + s
    raise TypeError("unknown project option")


@dataclasses.dataclass(frozen=True)
class ProjectLattice:
    """A Cox-Ross-Rubinstein lattice for the project value."""

    value: float = 0.0
    volatility: float = 0.0
    rate: float = 0.0
    payout: float = 0.0
    maturity: float = 0.0
    steps: int = 0


def _crr(lat: ProjectLattice) -> tuple[float, float, float, float]:
    if (
        not lat.value > 0.0
        or not lat.volatility > 0.0
        or not lat.maturity > 0.0
        or lat.steps <= 0
    ):
        raise ValueError("invalid lattice")
    dt = lat.maturity / lat.steps
    up = math.exp(lat.volatility * math.sqrt(dt))
    down = 1.0 / up
    p = (math.exp((lat.rate - lat.payout) * dt) - down) / (up - down)
    if not 0.0 < p < 1.0:
        raise ValueError("invalid lattice probability")
    return up, down, p, math.exp(-lat.rate * dt)


def _rollback(
    lat: ProjectLattice,
    carry: float,
    terminal: Callable[[np.ndarray], np.ndarray],
    exercise: Callable[[np.ndarray], np.ndarray],
) -> float:
    up, down, p, disc = _crr(lat)
    n = lat.steps
    k = np.arange(n + 1, dtype=float)
    values = terminal(lat.value * up**k * down ** (n - k))
    for i in range(n - 1, -1, -1):
        k = np.arange(i + 1, dtype=float)
        v = lat.value * up**k * down ** (i - k)
        hold = disc * (p * values[1:] + (1.0 - p) * values[:-1]) + carry * v
        values = np.maximum(hold, exercise(v))
    return float(values[0])


def project_with_options(
    lattice: ProjectLattice, options: Sequence[ProjectOption]
) -> float:
    """Project plus American-style options, each exercisable once.

    Raises:
        ValueError: For bad inputs or invalid probabilities.
    """

    def exercise(v: np.ndarray) -> np.ndarray:
        out = np.full(v.shape, -math.inf)
        for o in options:
            out = np.maximum(out, exercise_value(o, v))  # type: ignore[arg-type]
        return out

    dt = lattice.maturity / lattice.steps if lattice.steps > 0 else 0.0
    carry = 1.0 - math.exp(-lattice.payout * dt)
    return _rollback(
        lattice, carry, lambda v: np.maximum(v, exercise(v)), exercise
    )


def deferral_option(lattice: ProjectLattice, cost: float) -> float:
    """The option to defer an investment: an American call on V.

    Raises:
        ValueError: For bad inputs.
    """

    def payoff(v: np.ndarray) -> np.ndarray:
        return np.maximum(v - cost, 0.0)

    return _rollback(lattice, 0.0, payoff, payoff)


@dataclasses.dataclass(frozen=True)
class ExtractionProject:
    """Cash flow production (S - variable_cost) - fixed_cost a period."""

    production: float = 0.0
    variable_cost: float = 0.0
    fixed_cost: float = 0.0
    rate: float = 0.0
    abandon_value: float = 0.0
    expand_factor: float = 1.0
    expand_cost: float = 0.0


@dataclasses.dataclass(frozen=True)
class ProjectValuation:
    """Project value without options and with each option."""

    base: float
    with_abandon: float
    with_expand: float


def value_extraction_project(
    tree: CommodityTree, project: ExtractionProject
) -> ProjectValuation:
    """Values the project on the commodity tree (Ch 36.5)."""
    n = tree.steps

    def cash_flow(step: int, j: int) -> float:
        return (
            project.production * (tree.price(step, j) - project.variable_cost)
            - project.fixed_cost
        )

    wn = tree.width(n)
    base = [cash_flow(n, j) for j in range(-wn, wn + 1)]
    abandon = [max(cf, project.abandon_value) for cf in base]
    expand = [
        max(cf, project.expand_factor * cf - project.expand_cost) for cf in base
    ]
    for i in range(n - 1, -1, -1):
        base = tree.rollback(base, i, project.rate)
        abandon = tree.rollback(abandon, i, project.rate)
        expand = tree.rollback(expand, i, project.rate)
        w = tree.width(i)
        for j in range(-w, w + 1):
            k = j + w
            cf = cash_flow(i, j) if i > 0 else 0.0
            base[k] += cf
            abandon[k] = max(abandon[k] + cf, project.abandon_value)
            expand[k] = max(
                expand[k] + cf,
                project.expand_factor * base[k] - project.expand_cost,
            )
    return ProjectValuation(base[0], abandon[0], expand[0])


@dataclasses.dataclass(frozen=True)
class BusinessSpec:
    """Revenue GBM, costs, cash and a terminal multiple (Ch 36.4)."""

    revenue: float = 0.0
    growth: float = 0.0
    volatility: float = 0.0
    variable_cost: float = 0.0
    fixed_cost: float = 0.0
    cash: float = 0.0
    rate: float = 0.0
    years: float = 0.0
    steps_per_year: int = 4
    terminal_multiple: float = 0.0


@dataclasses.dataclass(frozen=True)
class BusinessValue:
    """Value, its standard error and the probability of bankruptcy."""

    value: float
    std_error: float
    bankruptcy_probability: float


def value_business(spec: BusinessSpec, paths: int, seed: int) -> BusinessValue:
    """Monte Carlo value of a business that fails once cash goes negative.

    The C++ version draws from mt19937_64, this one from NumPy.

    Raises:
        ValueError: For bad inputs.
    """
    if (
        not spec.revenue > 0.0
        or spec.volatility < 0.0
        or not spec.years > 0.0
        or spec.steps_per_year <= 0
        or paths < 2
    ):
        raise ValueError("invalid business inputs")
    steps = round(spec.years * spec.steps_per_year)
    if steps <= 0:
        raise ValueError("no steps")
    dt = spec.years / steps
    drift = (spec.growth - 0.5 * spec.volatility**2) * dt
    shock = spec.volatility * math.sqrt(dt)
    growth_cash = math.exp(spec.rate * dt)
    rng = np.random.default_rng(seed)
    revenue = np.full(paths, spec.revenue)
    cash = np.full(paths, spec.cash)
    last_flow = np.zeros(paths)
    alive = np.ones(paths, dtype=bool)
    for _ in range(steps):
        revenue = revenue * np.exp(drift + shock * rng.standard_normal(paths))
        flow = (revenue * (1.0 - spec.variable_cost) - spec.fixed_cost) * dt
        cash = np.where(alive, cash * growth_cash + flow, cash)
        last_flow = np.where(alive, flow / dt, last_flow)
        alive &= cash >= 0.0
    value = np.where(
        alive,
        math.exp(-spec.rate * spec.years)
        * (cash + spec.terminal_multiple * np.maximum(last_flow, 0.0)),
        0.0,
    )
    return BusinessValue(
        float(value.mean()),
        float(value.std(ddof=1)) / math.sqrt(paths),
        float(1.0 - alive.mean()),
    )
