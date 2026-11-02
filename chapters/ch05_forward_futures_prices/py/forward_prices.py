"""Chapter 5: forward prices, valuing forwards and spotting arbitrage.

Run with `python chapters/ch05_forward_futures_prices/py/forward_prices.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces the Chapter 5 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers

    f0 = p.forward_price(40.0, 0.05, 0.25)
    print(f"No income: S0 = 40, r = 5%, T = 0.25 -> F0 = {f0:.2f}")
    for quote in (43.0, 39.0, f0):
        verdict = p.detect_forward_arbitrage(quote, f0)
        print(f"  quoted {quote:.2f}: {verdict.name}")

    curve = lib.curves.ZeroCurve([1 / 3, 0.75], [0.03, 0.04])
    income = p.present_value([lib.instruments.CashFlow(1 / 3, 40.0)], curve)
    print(
        f"Known income: I = {income:.2f}, "
        f"F0 = {p.forward_price_with_income(900.0, income, 0.04, 0.75):.2f}"
    )

    c = lib.core.Compounding
    q = lib.core.equivalent_rate(0.04, c.periodic(2), c.continuous())
    print(
        f"Known yield: q = {q:.4f} -> "
        f"F0 = {p.forward_price_with_yield(25.0, 0.10, q, 0.5):.2f}"
    )

    f = p.forward_price(25.0, 0.10, 0.5)
    value = p.forward_contract_value(
        f, 24.0, 0.10, 0.5, lib.instruments.Side.LONG
    )
    print(f"Valuing a forward: F0 = {f:.2f} -> long value {value:.2f}")


if __name__ == "__main__":
    main()
