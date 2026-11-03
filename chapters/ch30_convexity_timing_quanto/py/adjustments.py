"""Chapter 30: convexity, timing and quanto adjustments.

Run with `python chapters/ch30_convexity_timing_quanto/py/adjustments.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math
from typing import Any

_PATHS = 200_000


def _book_examples(lib: Any) -> None:
    p = lib.pricers
    adj = p.bond_yield_convexity_adjustment(0.1, 0.2, 3.0, 0.0, 1.0, 1)
    print(
        f"Example 30.1: 1-year rate in 3 years: adjustment {100 * adj:.4f}%, "
        f"expected rate {100 * (0.1 + adj):.4f}%, value "
        f"{1000 * (0.1 + adj) / math.pow(1.1, 3):.2f}"
    )
    factor = p.timing_adjustment_factor(0.1, 0.2, 0.2, 1.0, 3.0, 1.0)
    rate = (0.1 + adj) * factor
    print(
        f"Example 30.3: paid in 4 years instead: timing factor {factor:.6f}, "
        f"expected rate {100 * rate:.4f}%, value "
        f"{1000 * rate / math.pow(1.1, 4):.2f}"
    )
    swap = p.bond_yield_convexity_adjustment(0.12, 0.22, 4.0, 0.12, 3.0, 1)
    print(
        "3-year swap rate in 4 years, forward 12%, 22% vol: adjustment "
        f"{100 * swap:.4f}%"
    )


def _adjustment_table(lib: Any) -> None:
    p = lib.pricers
    print(
        "Adjustments (bp) by observation time T, 5-year annual swap rate at 5%"
    )
    print("      T  convexity 15%  convexity 30%  quanto rho=-0.3")
    for t in (1.0, 2.0, 5.0, 10.0, 20.0):
        c15 = p.bond_yield_convexity_adjustment(0.05, 0.15, t, 0.05, 5.0, 1)
        c30 = p.bond_yield_convexity_adjustment(0.05, 0.30, t, 0.05, 5.0, 1)
        quanto = 0.05 * (p.quanto_adjustment_factor(-0.3, 0.12, 0.15, t) - 1)
        print(
            f"  {t:5.0f}  {1e4 * c15:13.2f}  {1e4 * c30:13.2f}  "
            f"{1e4 * quanto:15.2f}"
        )


def _siegel(lib: Any) -> None:
    f, sigma, t = 1.10, 0.15, 2.0
    rng = lib.random.NormalGenerator(7)
    usd_s = usd_inverse = eur_inverse = 0.0
    for _ in range(_PATHS):
        z = rng.next()
        s = f * math.exp(-0.5 * sigma * sigma * t + sigma * math.sqrt(t) * z)
        usd_s += s
        usd_inverse += 1.0 / s
        eur_inverse += 1.0 / (s * math.exp(sigma * sigma * t))
    print(
        f"Siegel's paradox, F = {f:.2f}, 15% vol, 2 years: USD measure E[S] = "
        f"{usd_s / _PATHS:.4f}, E[1/S] = {usd_inverse / _PATHS:.4f} vs 1/F = "
        f"{1 / f:.4f} (theory {math.exp(sigma * sigma * t) / f:.4f}); EUR "
        f"measure E[1/S] = {eur_inverse / _PATHS:.4f}"
    )


def main() -> None:
    """Reproduces the Chapter 30 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    _book_examples(lib)
    _adjustment_table(lib)
    _siegel(lib)


if __name__ == "__main__":
    main()
