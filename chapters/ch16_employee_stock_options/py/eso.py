"""Chapter 16: employee stock options.

Run with `python chapters/ch16_employee_stock_options/py/eso.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces the Chapter 16 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers
    market = p.TreeMarket(spot=40.0, rate=0.05, yield_=0.0, volatility=0.30)
    print(
        "Hull-White ESO, S0 = K = 40, 10 years, vesting 3 years, "
        "r = 5%, sigma = 30%"
    )
    print("  multiple M    exit 0%   exit 4%   exit 8%")
    for m in (1.2, 1.5, 2.0, 3.0, 1e12):
        row = "  never   " if m > 1e6 else f"  {m:<8.1f}"
        for exit_rate in (0.0, 0.04, 0.08):
            terms = p.EsoTerms(40.0, 10.0, 3.0, m, exit_rate)
            v = p.price_employee_stock_option(terms, market, 1000)
            row += f"{v:>10.2f}"
        print(row)
    call = lib.instruments.OptionType.CALL
    bsm = p.bsm_price(call, 40.0, 40.0, 0.05, 0.0, 0.30, 10.0)
    print(f"European 10-year call (BSM): {bsm:.2f}")
    print("Expected-life approach")
    for life in (4.0, 6.5, 10.0):
        v = p.expected_life_value(40.0, 40.0, 0.05, 0.0, 0.30, life)
        print(f"  expected life {life:>4.1f} years: {v:.2f}")
    w = p.warrant_price(40.0, 60.0, 0.03, 0.30, 5.0, 1_000_000, 200_000)
    print(
        "Dilution: 200,000 options at K = 60 on 1,000,000 shares at 40 are "
        f"worth {w:.2f} each"
    )


if __name__ == "__main__":
    main()
