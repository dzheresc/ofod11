"""Chapter 8: losses on ABS tranches for a range of pool losses.

Run with `python chapters/ch08_securitization/py/waterfall.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Prints the tranche loss table."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    c = lib.credit
    tranches = [c.Tranche(0.0, 0.05), c.Tranche(0.05, 0.20), c.Tranche(0.20)]
    print("Pool loss  Equity  Mezz.  Senior")
    for pool in (0.02, 0.05, 0.10, 0.15, 0.20, 0.25, 0.35):
        e, m, s = c.waterfall_losses(tranches, pool)
        print(
            f"{100 * pool:>8.0f}% {100 * e:>6.1f}% {100 * m:>5.1f}% "
            f"{100 * s:>6.1f}%"
        )


if __name__ == "__main__":
    main()
