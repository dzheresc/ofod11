"""Chapter 31: equilibrium short-rate models.

Run with `python chapters/ch31_short_rate_models/py/short_rate.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math
from typing import Any

_PATHS = 5_000


def _term_structures(lib: Any) -> None:
    md = lib.models
    vasicek = md.Vasicek(0.1, 0.05, 0.015)
    cir = md.Cir(0.1, 0.05, 0.07)
    print(
        "Zero rates (%) implied by Vasicek (a = 0.1, b = 5%, sigma = 1.5%) "
        "and CIR (sigma = 0.07)"
    )
    print("  model    r0      1y      2y      5y     10y     30y")
    for r0 in (0.02, 0.05, 0.08):
        for name, model in (("Vasicek", vasicek), ("CIR", cir)):
            row = f"  {name:7} {100 * r0:4.0f}%"
            for tau in (1.0, 2.0, 5.0, 10.0, 30.0):
                row += f"  {100 * md.zero_rate(model, r0, tau):6.3f}"
            print(row)
    call = md.zero_bond_option_price(
        vasicek, lib.instruments.OptionType.CALL, 0.05, 1.0, 5.0, 0.80
    )
    print(
        f"Vasicek 1-year call on a 5-year zero, K = 0.80, r0 = 5%: {call:.6f}"
    )


def _estimation(lib: Any) -> None:
    md = lib.models
    a, b, sigma = 0.3, 0.04, 0.01
    dt = 1.0 / 252.0
    phi = math.exp(-a * dt)
    sd = sigma * math.sqrt((1.0 - phi * phi) / (2.0 * a))
    rng = lib.random.NormalGenerator(17)
    rates = [0.03]
    for _ in range(252 * 10):
        rates.append(b + (rates[-1] - b) * phi + sd * rng.next())
    fit = md.estimate_vasicek(rates, dt)
    print(
        "Vasicek fit to 10 years of daily rates (true a = 0.3, b = 4%, sigma "
        f"= 1%): a = {fit.a:.4f}, b = {100 * fit.b:.4f}%, sigma = "
        f"{100 * fit.sigma:.4f}%"
    )


def _cir_simulation(lib: Any) -> None:
    md = lib.models
    cir = md.Cir(0.2, 0.05, 0.1)
    sum_rate = sum_bond = 0.0
    for p in range(_PATHS):
        path = md.simulate_cir(cir, 0.03, 0.05, 100, p)
        integral = sum(
            0.5 * (x + y) * 0.05 for x, y in zip(path, path[1:], strict=False)
        )
        sum_rate += path[-1]
        sum_bond += math.exp(-integral)
    print(
        "CIR exact simulation (a = 0.2, b = 5%, sigma = 0.1, r0 = 3%), 5 "
        f"years: E[r] {100 * sum_rate / _PATHS:.4f}% (theory "
        f"{100 * (0.05 + (0.03 - 0.05) * math.exp(-1.0)):.4f}%), bond "
        f"{sum_bond / _PATHS:.5f} (closed form "
        f"{md.bond_price(cir, 0.03, 5.0):.5f})"
    )


def main() -> None:
    """Reproduces the Chapter 31 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    _term_structures(lib)
    _estimation(lib)
    _cir_simulation(lib)


if __name__ == "__main__":
    main()
