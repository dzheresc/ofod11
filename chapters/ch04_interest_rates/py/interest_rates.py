"""Chapter 4: compounding, bonds, bootstrapping, forwards and duration.

Run with `python chapters/ch04_interest_rates/py/interest_rates.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces Tables 4.1 and 4.3-4.6."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    core, curves, ins, pricers = (
        lib.core,
        lib.curves,
        lib.instruments,
        lib.pricers,
    )

    print("Table 4.1: $100 at 10% for one year")
    for m in (1, 2, 4, 12, 52, 365):
        g = core.growth_factor(0.10, 1.0, core.Compounding.periodic(m))
        print(f"  m = {m:3d}: {100 * g:.2f}")

    curve = curves.ZeroCurve([0.5, 1.0, 1.5, 2.0], [0.05, 0.058, 0.064, 0.068])
    bond = ins.Bond(coupon_rate=0.06, maturity=2.0)
    price = pricers.bond_price(bond, curve)
    print(
        f"\nTable 4.2 curve, 2-year 6% bond: price {price:.2f}, "
        f"yield {pricers.bond_yield(bond, price):.4f}, "
        f"par yield {pricers.par_yield(2.0, 2, curve):.4f}"
    )

    quotes = [
        curves.BondQuote(ins.Bond(maturity=0.25, frequency=4), 97.5),
        curves.BondQuote(ins.Bond(maturity=0.50), 94.9),
        curves.BondQuote(ins.Bond(maturity=1.00, frequency=1), 90.0),
        curves.BondQuote(ins.Bond(coupon_rate=0.08, maturity=1.5), 96.0),
        curves.BondQuote(ins.Bond(coupon_rate=0.12, maturity=2.0), 101.6),
    ]
    boot = curves.bootstrap_from_bonds(quotes)
    print("\nTable 4.4: bootstrapped zero rates (continuous)")
    for t, r in zip(boot.times, boot.zero_rates, strict=True):
        print(f"  {t:.2f} y: {100 * r:.3f}%")

    t45 = curves.ZeroCurve([1, 2, 3, 4, 5], [0.03, 0.04, 0.046, 0.05, 0.053])
    print("\nTable 4.5: forward rates")
    for n in range(2, 6):
        print(f"  year {n}: {100 * t45.forward_rate(n - 1, n):.1f}%")

    bond3 = ins.Bond(coupon_rate=0.10, maturity=3.0)
    print(
        "\nTable 4.6: 3-year 10% bond at 12%: "
        f"price {pricers.bond_price_from_yield(bond3, 0.12):.3f}, "
        f"duration {pricers.macaulay_duration(bond3, 0.12):.3f}, "
        f"convexity {pricers.convexity(bond3, 0.12):.3f}"
    )


if __name__ == "__main__":
    main()
