"""Chapter 13: binomial trees, American exercise and convergence to BSM.

Run with `python chapters/ch13_binomial_trees/py/binomial_trees.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
Pass --plot to draw the convergence of the three schemes (needs matplotlib).
"""

import argparse
import importlib
import math
from typing import Any

_BSM = 4.7594


def _plot(lib: Any, option: Any, market: Any) -> None:
    from matplotlib import pyplot  # noqa: PLC0415

    p = lib.pricers
    steps = list(range(5, 201))
    for scheme in (
        p.TreeScheme.COX_ROSS_RUBINSTEIN,
        p.TreeScheme.JARROW_RUDD,
        p.TreeScheme.TIAN,
    ):
        prices = [
            p.price_binomial(option, market, n, scheme).price for n in steps
        ]
        pyplot.plot(steps, prices, label=scheme.name, linewidth=0.8)
    pyplot.axhline(_BSM, color="black", linewidth=0.5, label="BSM")
    pyplot.xlabel("steps")
    pyplot.ylabel("call price")
    pyplot.legend()
    pyplot.show()


def main() -> None:
    """Reproduces the Chapter 13 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    parser.add_argument("--plot", action="store_true", help="draw convergence")
    args = parser.parse_args()
    lib = importlib.import_module("ofod_cpp" if args.cpp else "ofod")
    ins = lib.instruments
    p = lib.pricers

    call = ins.VanillaOption(strike=21.0, maturity=0.25)
    prob = p.risk_neutral_probability(1.1, 0.9, math.exp(0.03))
    r = p.price_tree(call, 20.0, 0.12, 1, p.TreeStep(1.1, 0.9, prob))
    print(f"One step: p = {prob:.4f}, call {r.price:.3f}, delta {r.delta:.2f}")

    q = p.risk_neutral_probability(1.2, 0.8, math.exp(0.05))
    step = p.TreeStep(1.2, 0.8, q)
    prices = []
    for ex in (ins.Exercise.EUROPEAN, ins.Exercise.AMERICAN):
        put = ins.VanillaOption(ins.OptionType.PUT, ex, 52.0, 2.0)
        prices.append(p.price_tree(put, 50.0, 0.05, 2, step).price)
    print(f"Two-step put: European {prices[0]:.4f}, American {prices[1]:.4f}")

    option = ins.VanillaOption(strike=40.0, maturity=0.5)
    market = p.TreeMarket(spot=42.0, rate=0.10, volatility=0.20)
    print(f"Steps      CRR       JR     Tian   (BSM {_BSM})")
    schemes = (
        p.TreeScheme.COX_ROSS_RUBINSTEIN,
        p.TreeScheme.JARROW_RUDD,
        p.TreeScheme.TIAN,
    )
    for n in (10, 11, 50, 51, 100, 500):
        cells = " ".join(
            f"{p.price_binomial(option, market, n, s).price:>8.4f}"
            for s in schemes
        )
        print(f"{n:>5} {cells}")
    if args.plot:
        _plot(lib, option, market)


if __name__ == "__main__":
    main()
