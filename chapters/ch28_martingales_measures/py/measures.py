"""Chapter 28: martingales and measures.

Run with `python chapters/ch28_martingales_measures/py/measures.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
from collections.abc import Callable
import importlib
import math
from typing import Any

_PATHS = 100_000


def _simulate(
    lib: Any, seed: int, draw: Callable[[Any], float]
) -> tuple[float, float]:
    rng = lib.random.NormalGenerator(seed)
    total = total_sq = 0.0
    for _ in range(_PATHS):
        x = draw(rng)
        total += x
        total_sq += x * x
    mean = total / _PATHS
    return mean, math.sqrt((total_sq / _PATHS - mean * mean) / (_PATHS - 1))


def main() -> None:
    """Reproduces the Chapter 28 experiments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers
    call = lib.instruments.OptionType.CALL

    mu, sigma, r = 0.12, 0.2, 0.05
    lam = (mu - r) / sigma
    g = lib.greeks.bsm_greeks(
        call, 100.0, lib.greeks.OptionInputs(100.0, r, 0.0, sigma, 1.0)
    )
    c = p.bsm_price(call, 100, 100, r, 0, sigma, 1.0)
    option_vol = sigma * 100.0 * g.delta / c
    print(
        f"Market price of risk {lam:.4f}: the call has volatility "
        f"{100 * option_vol:.2f}% and so expected return "
        f"{100 * (r + lam * option_vol):.2f}% = r + lambda sigma_c"
    )

    s0, k, v, t = 100.0, 100.0, 0.2, 1.0
    vt = v * math.sqrt(t)
    bsm = p.bsm_price(call, s0, k, r, 0.0, v, t)

    def bank(rng: Any) -> float:
        s = s0 * math.exp((r - 0.5 * v * v) * t + vt * rng.next())
        return math.exp(-r * t) * max(s - k, 0.0)

    def stock(rng: Any) -> float:
        s = s0 * math.exp((r + 0.5 * v * v) * t + vt * rng.next())
        return s0 * max(s - k, 0.0) / s

    forward = s0 * math.exp(r * t)

    def bond(rng: Any) -> float:
        f = forward * math.exp(-0.5 * v * v * t + vt * rng.next())
        return math.exp(-r * t) * max(f - k, 0.0)

    print(f"1-year ATM call, S = 100, r = 5%, sigma = 20%: BSM {bsm:.4f}")
    for name, seed, draw in (
        ("money market numeraire ", 1, bank),
        ("stock numeraire        ", 2, stock),
        ("zero-coupon bond       ", 3, bond),
    ):
        mean, se = _simulate(lib, seed, draw)
        print(f"  {name} {mean:.4f} (se {se:.4f})")

    a = p.ExchangeAssets(100.0, 95.0, 0.02, 0.01, 0.3, 0.2, 0.4)
    rho_bar = math.sqrt(1.0 - a.correlation**2)

    def both(rng: Any) -> float:
        z1 = rng.next()
        z2 = a.correlation * z1 + rho_bar * rng.next()
        rv = a.receive_spot * math.exp(
            (r - a.receive_yield - 0.5 * a.receive_vol * a.receive_vol) * t
            + a.receive_vol * math.sqrt(t) * z1
        )
        ru = a.deliver_spot * math.exp(
            (r - a.deliver_yield - 0.5 * a.deliver_vol * a.deliver_vol) * t
            + a.deliver_vol * math.sqrt(t) * z2
        )
        return math.exp(-r * t) * max(rv - ru, 0.0)

    var = (
        a.receive_vol * a.receive_vol
        + a.deliver_vol * a.deliver_vol
        - 2.0 * a.correlation * a.receive_vol * a.deliver_vol
    )

    def ratio(rng: Any) -> float:
        x = (
            a.receive_spot
            / a.deliver_spot
            * math.exp(
                (a.deliver_yield - a.receive_yield - 0.5 * var) * t
                + math.sqrt(var * t) * rng.next()
            )
        )
        return (
            a.deliver_spot * math.exp(-a.deliver_yield * t) * max(x - 1.0, 0.0)
        )

    b_mean, b_se = _simulate(lib, 4, both)
    r_mean, r_se = _simulate(lib, 5, ratio)
    print(
        "Exchange option (Ch 26.14): Margrabe "
        f"{p.exchange_option_price(a, t):.4f}, risk-neutral MC {b_mean:.4f} "
        f"(se {b_se:.4f}), U-numeraire MC "
        f"{r_mean:.4f} (se {r_se:.4f})"
    )


if __name__ == "__main__":
    main()
