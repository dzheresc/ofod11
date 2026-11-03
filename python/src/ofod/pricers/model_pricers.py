"""European options under CEV, jump-diffusion and Heston (Hull, Ch 27).

The Monte Carlo stream comes from NumPy and differs from the C++
library's, so simulated prices agree only within standard error.
"""

import dataclasses
import math

import numpy as np

from ofod.instruments.option import OptionType
from ofod.math import quadrature
from ofod.math import special
from ofod.pricers import binomial_tree
from ofod.pricers import black_scholes
from ofod.pricers import monte_carlo

TreeMarket = binomial_tree.TreeMarket


def _put_from_call(
    call: float, m: TreeMarket, strike: float, t: float
) -> float:
    return (
        call - m.spot * math.exp(-m.yield_ * t) + strike * math.exp(-m.rate * t)
    )


def cev_price(
    option_type: OptionType,
    m: TreeMarket,
    strike: float,
    t: float,
    alpha: float,
) -> float:
    """CEV price for dS = (r - q) S dt + sigma S^alpha dz (Ch 27.1).

    Raises:
        ValueError: Unless alpha > 0 and |alpha - 1| >= 0.01.
    """
    if not alpha > 0.0 or abs(alpha - 1.0) < 0.01:
        raise ValueError("need alpha > 0 and |alpha - 1| >= 0.01")
    b_rate = m.rate - m.yield_
    sigma = m.volatility
    if b_rate == 0.0:
        v = sigma * sigma * t
    else:
        v = (
            sigma
            * sigma
            / (2.0 * b_rate * (alpha - 1.0))
            * (math.exp(2.0 * b_rate * (alpha - 1.0) * t) - 1.0)
        )
    one = 1.0 - alpha
    a = (strike * math.exp(-b_rate * t)) ** (2.0 * one) / (one * one * v)
    b = 1.0 / one
    c = m.spot ** (2.0 * one) / (one * one * v)
    s = m.spot * math.exp(-m.yield_ * t)
    k = strike * math.exp(-m.rate * t)
    chi = special.noncentral_chi_square_cdf
    if alpha < 1.0:
        call = s * (1.0 - chi(a, b + 2.0, c)) - k * chi(c, b, a)
    else:
        call = s * (1.0 - chi(c, -b, a)) - k * chi(a, 2.0 - b, c)
    if option_type is OptionType.CALL:
        return call
    return _put_from_call(call, m, strike, t)


@dataclasses.dataclass(frozen=True)
class JumpParams:
    """Jump intensity, expected proportional jump and log-jump volatility."""

    intensity: float
    mean_jump: float
    jump_vol: float


def merton_jump_price(
    option_type: OptionType,
    m: TreeMarket,
    strike: float,
    t: float,
    jumps: JumpParams,
) -> float:
    """Merton's jump diffusion: Poisson-weighted BSM prices (Ch 27.1)."""
    lambda_t = jumps.intensity * (1.0 + jumps.mean_jump) * t
    if not lambda_t > 0.0:
        return black_scholes.bsm_price(
            option_type, m.spot, strike, m.rate, m.yield_, m.volatility, t
        )
    gamma = math.log1p(jumps.mean_jump)
    total = 0.0
    for n in range(500):
        weight = math.exp(
            -lambda_t + n * math.log(lambda_t) - math.lgamma(n + 1.0)
        )
        vol = math.sqrt(m.volatility**2 + n * jumps.jump_vol**2 / t)
        rate = m.rate - jumps.intensity * jumps.mean_jump + n * gamma / t
        total += weight * black_scholes.bsm_price(
            option_type, m.spot, strike, rate, m.yield_, vol, t
        )
        if n > lambda_t and weight < 1e-17:
            break
    return total


@dataclasses.dataclass(frozen=True)
class HestonParams:
    """v0, kappa, theta, sigma (vol of variance) and rho."""

    v0: float
    kappa: float
    theta: float
    sigma: float
    rho: float


def _heston_exponent(xi: np.ndarray, t: float, p: HestonParams) -> np.ndarray:
    """C(xi) + D(xi) v0: the log of E[exp(i xi ln(S_T / F))], little trap."""
    s2 = p.sigma * p.sigma
    beta = p.kappa - p.rho * p.sigma * 1j * xi
    d = np.sqrt(beta * beta + s2 * (1j * xi + xi * xi))
    g = (beta - d) / (beta + d)
    e = np.exp(-d * t)
    c = (
        p.kappa
        * p.theta
        / s2
        * ((beta - d) * t - 2.0 * np.log((1.0 - g * e) / (1.0 - g)))
    )
    dd = (beta - d) / s2 * (1.0 - e) / (1.0 - g * e)
    out: np.ndarray = c + dd * p.v0
    return out


_MAX_PANELS = 20000


def heston_price(
    option_type: OptionType,
    m: TreeMarket,
    strike: float,
    t: float,
    params: HestonParams,
) -> float:
    """Heston's semi-analytic price (Ch 27.2); m.volatility is ignored.

    Same method as the C++ library: Lewis's single integral with a
    Black-Scholes control variate at the expected average variance,
    16-point Gauss-Legendre panels growing from width 1/2 to a width set by
    the integrand's decay and oscillation, until three consecutive panels
    add less than 1e-14 (1 + |integral|).
    """
    p = params
    rule = quadrature.gauss_legendre(16, 0.0, 1.0)
    nodes = np.asarray(rule.nodes)
    weights = np.asarray(rule.weights)
    forward = m.spot * math.exp((m.rate - m.yield_) * t)
    x = math.log(forward / strike)
    kt = p.kappa * t
    mean_reversion = t if abs(kt) < 1e-8 else -math.expm1(-kt) / p.kappa
    w = max(p.theta * t + (p.v0 - p.theta) * mean_reversion, 1e-8)

    tail = (p.v0 + kt * p.theta) / p.sigma
    cap = 8.0 * math.sqrt(w)
    decay = min(tail * math.sqrt(max(1.0 - p.rho * p.rho, 0.0)), cap)
    phase = abs(x) + min(abs(p.rho) * tail, cap)
    width = min(4.0 / max(decay, math.sqrt(w), phase, 1e-3), 50.0)

    def integrand(u: np.ndarray) -> np.ndarray:
        z = _heston_exponent(u - 0.5j, t, p)
        value = np.exp(z.real) * np.cos(z.imag + u * x)
        bs = -0.5 * w * (u * u + 0.25)
        value -= np.where(bs > -60.0, np.exp(np.maximum(bs, -60.0)), 0.0) * (
            np.cos(u * x)
        )
        out: np.ndarray = value / (u * u + 0.25)
        return out

    # Panel widths 1/2, 1, 2, ... up to `width`, then constant.
    widths = [min(0.5 * 2.0**i, width) for i in range(64)]
    widths = [h for h in widths if h < width] + [width]
    integral, quiet, k, left = 0.0, 0, 0, 0.0
    batch = 32
    while k < _MAX_PANELS and quiet < 3:
        n = min(batch, _MAX_PANELS - k)
        h = np.array([widths[min(k + i, len(widths) - 1)] for i in range(n)])
        lefts = left + np.concatenate(([0.0], np.cumsum(h)[:-1]))
        u = (lefts[:, None] + h[:, None] * nodes[None, :]).ravel()
        parts = (integrand(u).reshape(n, -1) * weights).sum(axis=1) * h
        for part, hk in zip(parts, h, strict=True):
            integral += float(part)
            left += float(hk)
            k += 1
            quiet = (
                quiet + 1 if abs(part) < 1e-14 * (1.0 + abs(integral)) else 0
            )
            if quiet >= 3:
                break
    call = black_scholes.bsm_price(
        OptionType.CALL, m.spot, strike, m.rate, m.yield_, math.sqrt(w / t), t
    ) - math.sqrt(forward * strike) * math.exp(-m.rate * t) / math.pi * (
        integral
    )
    if option_type is OptionType.CALL:
        return call
    return _put_from_call(call, m, strike, t)


def heston_monte_carlo(
    option_type: OptionType,
    m: TreeMarket,
    strike: float,
    t: float,
    params: HestonParams,
    steps: int,
    paths: int,
    seed: int,
) -> monte_carlo.McResult:
    """Log-Euler for S, full-truncation Euler for v.

    Raises:
        ValueError: For steps < 1, paths < 2 or non-positive t or spot.
    """
    if steps < 1 or paths < 2 or not t > 0.0 or not m.spot > 0.0:
        raise ValueError("invalid simulation inputs")
    rng = np.random.default_rng(seed)
    dt = t / steps
    rho_bar = math.sqrt(1.0 - params.rho**2)
    x = np.full(paths, math.log(m.spot))
    v = np.full(paths, params.v0)
    for _ in range(steps):
        z1 = rng.standard_normal(paths)
        z2 = params.rho * z1 + rho_bar * rng.standard_normal(paths)
        vp = np.maximum(v, 0.0)
        root = np.sqrt(vp * dt)
        x += (m.rate - m.yield_ - 0.5 * vp) * dt + root * z1
        v += params.kappa * (params.theta - vp) * dt + params.sigma * root * z2
    s = np.exp(x)
    payoff = s - strike if option_type is OptionType.CALL else strike - s
    values = math.exp(-m.rate * t) * np.maximum(payoff, 0.0)
    return monte_carlo.McResult(
        float(values.mean()),
        float(values.std(ddof=1)) / math.sqrt(paths),
        paths,
    )
