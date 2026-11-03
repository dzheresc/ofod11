"""One-factor short-rate models (Hull, Ch 31 and 32).

Vasicek and CIR equilibrium models, and the no-arbitrage Ho-Lee and
Hull-White models fitted to today's zero curve.
"""

from collections.abc import Sequence
import dataclasses
import math

import numpy as np

from ofod.curves import zero_curve
from ofod.instruments.option import OptionType
from ofod.math import normal
from ofod.math import optimize
from ofod.math import roots

ZeroCurve = zero_curve.ZeroCurve
_MIN_A = 1e-6


@dataclasses.dataclass(frozen=True)
class Vasicek:
    """dr = a (b - r) dt + sigma dz."""

    a: float
    b: float
    sigma: float


@dataclasses.dataclass(frozen=True)
class Cir:
    """dr = a (b - r) dt + sigma sqrt(r) dz."""

    a: float
    b: float
    sigma: float


def bond_price(model: Vasicek | Cir, r: float, tau: float) -> float:
    """Returns P(t, t + tau) = A e^{-B r} (eqs. 31.6-31.8)."""
    a, b_, s = model.a, model.b, model.sigma
    if isinstance(model, Vasicek):
        b = (1.0 - math.exp(-a * tau)) / a
        log_a = (b - tau) * (a * a * b_ - 0.5 * s * s) / (
            a * a
        ) - s * s * b * b / (4.0 * a)
        return math.exp(log_a - b * r)
    gamma = math.sqrt(a * a + 2.0 * s * s)
    e = math.expm1(gamma * tau)
    denom = (gamma + a) * e + 2.0 * gamma
    b = 2.0 * e / denom
    log_a = (
        2.0
        * a
        * b_
        / (s * s)
        * math.log(2.0 * gamma * math.exp(0.5 * (a + gamma) * tau) / denom)
    )
    return math.exp(log_a - b * r)


def zero_rate(model: Vasicek | Cir, r: float, tau: float) -> float:
    """Returns -ln P / tau."""
    return -math.log(bond_price(model, r, tau)) / tau


def zero_bond_option_formula(
    option_type: OptionType,
    p_expiry: float,
    p_maturity: float,
    strike: float,
    sigma_p: float,
) -> float:
    """Returns P(0,s) N(h) - K P(0,T) N(h - sigma_P) for a call."""
    phi = 1.0 if option_type is OptionType.CALL else -1.0
    h = math.log(p_maturity / (p_expiry * strike)) / sigma_p + 0.5 * sigma_p
    return phi * (
        p_maturity * normal.norm_cdf(phi * h)
        - strike * p_expiry * normal.norm_cdf(phi * (h - sigma_p))
    )


def zero_bond_option_price(
    model: Vasicek,
    option_type: OptionType,
    r: float,
    expiry: float,
    maturity: float,
    strike: float,
) -> float:
    """Returns a Vasicek option on a unit zero-coupon bond."""
    a = model.a
    sigma_p = (
        model.sigma
        / a
        * (1.0 - math.exp(-a * (maturity - expiry)))
        * math.sqrt(-math.expm1(-2.0 * a * expiry) / (2.0 * a))
    )
    return zero_bond_option_formula(
        option_type,
        bond_price(model, r, expiry),
        bond_price(model, r, maturity),
        strike,
        sigma_p,
    )


def estimate_vasicek(rates: Sequence[float], dt: float) -> Vasicek:
    """Fits the exact AR(1) discretisation by least squares (Ch 31.4).

    Raises:
        ValueError: For fewer than 3 rates or phi outside (0, 1).
    """
    if len(rates) < 3 or dt <= 0.0:
        raise ValueError("need at least 3 rates and dt > 0")
    x = np.asarray(rates[:-1], dtype=float)
    y = np.asarray(rates[1:], dtype=float)
    n = float(len(x))
    sx, sy = float(x.sum()), float(y.sum())
    sxx, sxy = float(x @ x), float(x @ y)
    phi = (n * sxy - sx * sy) / (n * sxx - sx * sx)
    if not 0.0 < phi < 1.0:
        raise ValueError("no mean reversion in the data")
    c = (sy - phi * sx) / n
    e = y - c - phi * x
    residual_var = float(e @ e) / n
    a = -math.log(phi) / dt
    return Vasicek(
        a, c / (1.0 - phi), math.sqrt(residual_var * 2.0 * a / (1.0 - phi**2))
    )


def simulate_cir(
    model: Cir, r0: float, dt: float, steps: int, seed: int
) -> list[float]:
    """Returns steps + 1 rates from exact noncentral chi-square draws."""
    rng = np.random.default_rng(seed)
    decay = math.exp(-model.a * dt)
    c = model.sigma**2 * (1.0 - decay) / (4.0 * model.a)
    dof = 4.0 * model.a * model.b / model.sigma**2
    path = [r0]
    r = r0
    for _ in range(steps):
        r = c * float(rng.noncentral_chisquare(dof, r * decay / c))
        path.append(r)
    return path


class HullWhite:
    """dr = (theta(t) - a r) dt + sigma dz fitted to a zero curve."""

    def __init__(self, curve: ZeroCurve, a: float, sigma: float) -> None:
        """Initializes the model; a = 0 is Ho-Lee.

        Raises:
            ValueError: Unless a >= 0 and sigma > 0.
        """
        if not a >= 0.0 or not sigma > 0.0:
            raise ValueError("need a >= 0 and sigma > 0")
        self._curve = curve
        self._a = a
        self._sigma = sigma

    @property
    def a(self) -> float:
        """Mean reversion."""
        return self._a

    @property
    def sigma(self) -> float:
        """Short-rate volatility."""
        return self._sigma

    @property
    def curve(self) -> ZeroCurve:
        """The fitted zero curve."""
        return self._curve

    def b(self, t: float, maturity: float) -> float:
        """Returns B(t, T)."""
        tau = maturity - t
        if self._a < 1e-10:
            return tau
        return -math.expm1(-self._a * tau) / self._a

    def instantaneous_forward(self, t: float) -> float:
        """Returns F(0, t) by central differences of ln P."""
        h = 1e-5
        lo = max(t - h, 0.0)
        hi = t + h
        c = self._curve
        return (
            math.log(c.discount_factor(lo)) - math.log(c.discount_factor(hi))
        ) / (hi - lo)

    def bond_price(self, t: float, maturity: float, r: float) -> float:
        """Returns P(t, T) given the short rate r at t (eq. 32.9)."""
        a, s = self._a, self._sigma
        b = self.b(t, maturity)
        if a < 1e-10:
            variance_term = s * s * t * b * b / 2.0
        else:
            variance_term = (
                s * s / (4.0 * a) * -math.expm1(-2.0 * a * t) * b * b
            )
        c = self._curve
        log_a = (
            math.log(c.discount_factor(maturity) / c.discount_factor(t))
            + b * self.instantaneous_forward(t)
            - variance_term
        )
        return math.exp(log_a - b * r)

    def bond_option_volatility(self, expiry: float, maturity: float) -> float:
        """Returns sigma_P (eq. 32.10)."""
        a, s = self._a, self._sigma
        if a < 1e-10:
            return s * (maturity - expiry) * math.sqrt(expiry)
        return (
            s
            * self.b(expiry, maturity)
            * math.sqrt(-math.expm1(-2.0 * a * expiry) / (2.0 * a))
        )

    def zero_bond_option(
        self,
        option_type: OptionType,
        expiry: float,
        maturity: float,
        strike: float,
    ) -> float:
        """Returns an option on a unit zero-coupon bond."""
        c = self._curve
        return zero_bond_option_formula(
            option_type,
            c.discount_factor(expiry),
            c.discount_factor(maturity),
            strike,
            self.bond_option_volatility(expiry, maturity),
        )

    def coupon_bond_option(
        self,
        option_type: OptionType,
        expiry: float,
        times: Sequence[float],
        amounts: Sequence[float],
        strike: float,
    ) -> float:
        """Returns a coupon bond option by Jamshidian's decomposition.

        Raises:
            ValueError: For mismatched cash flows or no critical rate.
        """
        if len(times) != len(amounts) or not times:
            raise ValueError("bad cash flows")

        def bond(r: float) -> float:
            return (
                sum(
                    a * self.bond_price(expiry, t, r)
                    for t, a in zip(times, amounts, strict=True)
                )
                - strike
            )

        critical = roots.brent(bond, -1.0, 2.0)
        if critical is None:
            raise ValueError("critical rate not found")
        return sum(
            a
            * self.zero_bond_option(
                option_type, expiry, t, self.bond_price(expiry, t, critical)
            )
            for t, a in zip(times, amounts, strict=True)
        )

    def caplet(
        self,
        option_type: OptionType,
        reset: float,
        accrual: float,
        strike: float,
        notional: float,
    ) -> float:
        """Returns a caplet (CALL) or floorlet as bond options."""
        growth = 1.0 + strike * accrual
        bond_type = (
            OptionType.PUT
            if option_type is OptionType.CALL
            else OptionType.CALL
        )
        return (
            notional
            * growth
            * self.zero_bond_option(
                bond_type, reset, reset + accrual, 1.0 / growth
            )
        )


@dataclasses.dataclass(frozen=True)
class CapletQuote:
    """A caplet with its market (Black) price per unit notional."""

    reset: float
    accrual: float
    strike: float
    market_price: float


@dataclasses.dataclass(frozen=True)
class HullWhiteFit:
    """Calibrated parameters and the RMS relative pricing error."""

    a: float
    sigma: float
    rms_relative_error: float


def calibrate_hull_white(
    curve: ZeroCurve,
    quotes: Sequence[CapletQuote],
    a0: float = 0.1,
    sigma0: float = 0.01,
) -> HullWhiteFit:
    """Fits (a, sigma) to caplet prices with Nelder-Mead (Ch 32.10).

    Raises:
        ValueError: For no quotes or non-positive starting values.
    """
    if not quotes or a0 <= 0.0 or sigma0 <= 0.0:
        raise ValueError("need quotes and positive starting values")

    def error(x: Sequence[float]) -> float:
        model = HullWhite(curve, math.exp(x[0]) + _MIN_A, math.exp(x[1]))
        return sum(
            (
                model.caplet(OptionType.CALL, q.reset, q.accrual, q.strike, 1.0)
                / q.market_price
                - 1.0
            )
            ** 2
            for q in quotes
        )

    best = optimize.nelder_mead(
        error, [math.log(a0), math.log(sigma0)], [0.5, 0.5]
    )
    return HullWhiteFit(
        math.exp(best.x[0]) + _MIN_A,
        math.exp(best.x[1]),
        math.sqrt(best.value / len(quotes)),
    )
