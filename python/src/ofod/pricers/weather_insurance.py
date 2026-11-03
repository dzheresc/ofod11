"""Weather and insurance derivatives (Hull, Ch 35.5-35.6).

Heating and cooling degree days, degree-day options priced by historical
burn analysis and by simulating a mean-reverting seasonal temperature
model, and catastrophe bonds and reinsurance layers on a compound Poisson
loss process. Both risks are taken to be uncorrelated with the market, so
they are priced as discounted real-world expectations. The Monte Carlo
functions draw from NumPy; the C++ versions use mt19937_64, so the two
agree only statistically.
"""

from collections.abc import Sequence
import dataclasses
import enum
import math

import numpy as np

from ofod.instruments.option import OptionType
from ofod.math import linalg
from ofod.math import roots

_YEAR_DAYS = 365.0
_OMEGA = 2.0 * math.pi / _YEAR_DAYS


class DegreeDayKind(enum.Enum):
    """Heating (HDD) or cooling (CDD) degree days."""

    HEATING = 0
    COOLING = 1


def degree_days(
    kind: DegreeDayKind, temperatures: Sequence[float], base: float = 65.0
) -> float:
    """Sum of max(base - T, 0) (heating) or max(T - base, 0) (cooling)."""
    if kind is DegreeDayKind.HEATING:
        return sum(max(base - t, 0.0) for t in temperatures)
    return sum(max(t - base, 0.0) for t in temperatures)


@dataclasses.dataclass(frozen=True)
class DegreeDayOption:
    """Pays tick * max(D - K, 0) (call) or tick * max(K - D, 0), capped."""

    option_type: OptionType = OptionType.CALL
    strike: float = 0.0
    tick: float = 1.0
    cap: float = math.inf


def degree_day_payoff(option: DegreeDayOption, degree_days: float) -> float:
    """Returns the option's payoff for a season's degree days."""
    if option.option_type is OptionType.CALL:
        intrinsic = max(degree_days - option.strike, 0.0)
    else:
        intrinsic = max(option.strike - degree_days, 0.0)
    return min(option.tick * intrinsic, option.cap)


@dataclasses.dataclass(frozen=True)
class ExpectationEstimate:
    """A discounted mean payoff and its standard error."""

    value: float
    std_error: float


def _estimate(samples: np.ndarray, discount: float) -> ExpectationEstimate:
    n = samples.size
    return ExpectationEstimate(
        discount * float(samples.mean()),
        discount * float(samples.std(ddof=1)) / math.sqrt(n),
    )


def burn_analysis(
    option: DegreeDayOption,
    season_degree_days: Sequence[float],
    rate: float,
    maturity: float,
) -> ExpectationEstimate:
    """The discounted average payoff over historical seasons.

    Raises:
        ValueError: For fewer than two seasons.
    """
    if len(season_degree_days) < 2:
        raise ValueError("need at least two seasons")
    payoffs = np.array(
        [degree_day_payoff(option, d) for d in season_degree_days]
    )
    return _estimate(payoffs, math.exp(-rate * maturity))


@dataclasses.dataclass(frozen=True)
class TemperatureModel:
    """T_d = mean + trend d + amplitude cos(2 pi (d - phase) / 365) + x_d.

    The deviation x follows x_{d+1} = phi x_d + sigma eps.
    """

    mean: float = 0.0
    trend: float = 0.0
    amplitude: float = 0.0
    phase: float = 0.0
    phi: float = 0.0
    sigma: float = 0.0

    def seasonal(self, day: float) -> float:
        """Returns the expected temperature on `day`."""
        return (
            self.mean
            + self.trend * day
            + self.amplitude * math.cos(_OMEGA * (day - self.phase))
        )


def fit_temperature_model(
    temperatures: Sequence[float], first_day: int = 0
) -> TemperatureModel:
    """Least squares on (1, d, cos, sin), then AR(1) on the residuals.

    Raises:
        ValueError: For fewer than 60 observations.
    """
    n = len(temperatures)
    if n < 60:
        raise ValueError("need at least 60 observations")
    d = first_day + np.arange(n, dtype=float)
    x = np.column_stack([np.ones(n), d, np.cos(_OMEGA * d), np.sin(_OMEGA * d)])
    y = np.asarray(temperatures, dtype=float)
    beta = linalg.solve_linear((x.T @ x).tolist(), (x.T @ y).tolist())
    phase = math.atan2(beta[3], beta[2]) / _OMEGA
    if phase < 0.0:
        phase += _YEAR_DAYS
    partial = TemperatureModel(
        beta[0], beta[1], math.hypot(beta[2], beta[3]), phase
    )
    resid = y - np.array([partial.seasonal(v) for v in d])
    sxx = float(resid[:-1] @ resid[:-1])
    if not sxx > 0.0:
        raise ValueError("residuals are all zero")
    phi = float(resid[:-1] @ resid[1:]) / sxx
    e = resid[1:] - phi * resid[:-1]
    sigma = math.sqrt(float(e @ e) / (n - 2))
    return dataclasses.replace(partial, phi=phi, sigma=sigma)


@dataclasses.dataclass(frozen=True)
class SeasonSpec:
    """The degree-day season: `days` days from `first_day`."""

    kind: DegreeDayKind = DegreeDayKind.HEATING
    first_day: int = 0
    days: int = 0
    last_deviation: float = 0.0
    base: float = 65.0


@dataclasses.dataclass(frozen=True)
class DegreeDaySimulation:
    """Option estimate and the simulated degree-day distribution."""

    option: ExpectationEstimate
    mean_degree_days: float
    std_degree_days: float


def simulate_degree_day_option(
    model: TemperatureModel,
    season: SeasonSpec,
    option: DegreeDayOption,
    rate: float,
    maturity: float,
    paths: int,
    seed: int,
) -> DegreeDaySimulation:
    """Simulates the season's temperatures and prices the option.

    Raises:
        ValueError: For days <= 0, paths < 2 or sigma < 0.
    """
    if season.days <= 0 or paths < 2 or model.sigma < 0.0:
        raise ValueError("invalid simulation inputs")
    rng = np.random.default_rng(seed)
    x = np.full(paths, season.last_deviation)
    total = np.zeros(paths)
    for k in range(season.days):
        x = model.phi * x + model.sigma * rng.standard_normal(paths)
        t = model.seasonal(season.first_day + k) + x
        if season.kind is DegreeDayKind.HEATING:
            total += np.maximum(season.base - t, 0.0)
        else:
            total += np.maximum(t - season.base, 0.0)
    payoffs = np.array([degree_day_payoff(option, v) for v in total])
    return DegreeDaySimulation(
        _estimate(payoffs, math.exp(-rate * maturity)),
        float(total.mean()),
        float(total.std(ddof=1)),
    )


@dataclasses.dataclass(frozen=True)
class LossModel:
    """Poisson(intensity per year) events with lognormal severities."""

    intensity: float = 0.0
    severity_mu: float = 0.0
    severity_sigma: float = 0.0


def _aggregate_losses(
    m: LossModel, years: float, paths: int, rng: np.random.Generator
) -> np.ndarray:
    if not m.intensity * years > 0.0:
        return np.zeros(paths)
    counts = rng.poisson(m.intensity * years, paths)
    sizes = np.exp(
        m.severity_mu + m.severity_sigma * rng.standard_normal(counts.sum())
    )
    owner = np.repeat(np.arange(paths), counts)
    return np.bincount(owner, sizes, paths)


@dataclasses.dataclass(frozen=True)
class CatBondTerms:
    """Principal written down as losses go from attachment to exhaustion."""

    principal: float = 100.0
    coupon: float = 0.0
    maturity: float = 1.0
    attachment: float = 0.0
    exhaustion: float = 0.0
    frequency: int = 1


@dataclasses.dataclass(frozen=True)
class CatBondValue:
    """Price, loss statistics and the spread over the risk-free rate."""

    price: float
    std_error: float
    expected_loss: float
    loss_probability: float
    spread: float


def price_cat_bond(
    terms: CatBondTerms, losses: LossModel, rate: float, paths: int, seed: int
) -> CatBondValue:
    """Monte Carlo value of a catastrophe bond.

    Coupons are paid on the original principal; the remaining principal is
    paid at maturity. The spread is the constant y - r that reprices the
    promised cash flows to the estimate.

    Raises:
        ValueError: For bad terms.
    """
    if (
        losses.intensity < 0.0
        or losses.severity_sigma < 0.0
        or paths < 2
        or not terms.maturity > 0.0
        or not terms.principal > 0.0
        or terms.frequency <= 0
        or not terms.exhaustion > terms.attachment
        or terms.attachment < 0.0
    ):
        raise ValueError("invalid cat bond inputs")
    coupons = max(1, round(terms.maturity * terms.frequency))
    coupon = terms.principal * terms.coupon / terms.frequency

    def promised(y: float) -> float:
        pv = sum(
            coupon * math.exp(-y * k / terms.frequency)
            for k in range(1, coupons + 1)
        )
        return pv + terms.principal * math.exp(-y * terms.maturity)

    disc = math.exp(-rate * terms.maturity)
    coupon_pv = promised(rate) - terms.principal * disc
    rng = np.random.default_rng(seed)
    loss = _aggregate_losses(losses, terms.maturity, paths, rng)
    fraction = np.clip(
        (loss - terms.attachment) / (terms.exhaustion - terms.attachment),
        0.0,
        1.0,
    )
    values = coupon_pv + disc * terms.principal * (1.0 - fraction)
    price = float(values.mean())
    y = roots.brent(lambda v: promised(v) - price, rate - 1.0, rate + 5.0)
    if y is None:
        raise ValueError("spread not found")
    return CatBondValue(
        price,
        float(values.std(ddof=1)) / math.sqrt(paths),
        float(fraction.mean()),
        float(np.mean(loss > terms.attachment)),
        y - rate,
    )


def excess_of_loss_layer(
    losses: LossModel,
    attachment: float,
    limit: float,
    years: float,
    rate: float,
    paths: int,
    seed: int,
) -> ExpectationEstimate:
    """Expected discounted payout min(max(L - attachment, 0), limit).

    Raises:
        ValueError: For bad inputs.
    """
    if (
        losses.intensity < 0.0
        or losses.severity_sigma < 0.0
        or paths < 2
        or not years > 0.0
        or limit < 0.0
        or attachment < 0.0
    ):
        raise ValueError("invalid layer inputs")
    rng = np.random.default_rng(seed)
    loss = _aggregate_losses(losses, years, paths, rng)
    payout = np.minimum(np.maximum(loss - attachment, 0.0), limit)
    return _estimate(payout, math.exp(-rate * years))
