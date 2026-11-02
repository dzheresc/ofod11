"""Chapter 7: interest rate and currency swaps.

Run with `python chapters/ch07_swaps/py/swaps.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces the Chapter 7 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers
    long = lib.instruments.Side.LONG
    curve = lib.curves.ZeroCurve([0.25, 0.75, 1.25], [0.10, 0.105, 0.11])
    swap = lib.instruments.InterestRateSwap(
        notional=100.0,
        fixed_rate=0.08,
        frequency=2,
        payment_times=[0.25, 0.75, 1.25],
        next_floating_rate=0.102,
    )
    print(
        f"IRS: B_fix {p.fixed_leg_bond_value(swap, curve):.2f}, "
        f"B_fl {p.floating_leg_bond_value(swap, curve):.2f}, "
        f"value {p.swap_value(swap, curve, long):.2f} "
        f"(as FRAs {p.swap_value_as_fras(swap, curve, long):.2f})"
    )
    ccy = lib.instruments.CurrencySwap(
        domestic_notional=10.0,
        domestic_rate=0.08,
        foreign_notional=1200.0,
        foreign_rate=0.05,
        frequency=1,
        payment_times=[1.0, 2.0, 3.0],
    )
    flat = lib.curves.ZeroCurve.flat
    value = p.currency_swap_value(
        ccy, flat(0.09), flat(0.04), 1.0 / 110.0, long
    )
    print(f"Currency swap value: {value:.4f} million USD")
    gain = 100.0 * p.comparative_advantage_gain(0.04, 0.052, -0.001, 0.006)
    print(f"Comparative advantage gain: {gain:.2f}%")


if __name__ == "__main__":
    main()
