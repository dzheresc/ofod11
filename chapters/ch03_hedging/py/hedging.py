"""Chapter 3: hedge ratios, contract counts and changing portfolio beta.

Run with `python chapters/ch03_hedging/py/hedging.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces the Chapter 3 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    r = lib.risk
    h = r.minimum_variance_hedge_ratio(0.928, 0.0263, 0.0313)
    print(
        f"Jet fuel: h* = {h:.4f}, effectiveness "
        f"{r.hedge_effectiveness(0.928):.4f}, contracts "
        f"{r.optimal_contracts(0.78, 2_000_000.0, 42_000.0):.2f}"
    )
    portfolio = 5_050_000.0
    futures = 1010.0 * 250.0
    n = r.beta_hedge_contracts(1.5, portfolio, futures)
    print(f"Beta 1.5 portfolio: hedge with {n:.0f} contracts")
    for target in (0.75, 2.0):
        n = r.change_beta_contracts(1.5, target, portfolio, futures)
        print(f"  to beta {target:.2f}: {n:+.0f} contracts")
    legs = [
        r.RollLeg(68.20, 67.40),
        r.RollLeg(67.00, 66.50),
        r.RollLeg(66.30, 65.90),
    ]
    print(f"Stack and roll gain per barrel: {r.short_roll_gain(legs):.2f}")


if __name__ == "__main__":
    main()
