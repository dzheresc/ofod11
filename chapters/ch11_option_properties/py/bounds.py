"""Chapter 11: bounds on option prices, put-call parity, early exercise.

Run with `python chapters/ch11_option_properties/py/bounds.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces the Chapter 11 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers
    low = p.european_call_lower_bound(20.0, 18.0, 0.10, 1.0)
    print(f"Call lower bound (S=20, K=18, r=10%, T=1): {low:.2f}")
    low = p.european_put_lower_bound(37.0, 40.0, 0.05, 0.5)
    print(f"Put lower bound (S=37, K=40, r=5%, T=0.5): {low:.2f}")
    parity = p.put_from_parity(3.0, 31.0, 30.0, 0.10, 0.25)
    print(
        f"Parity put for c=3 (S=31, K=30, r=10%, T=0.25): {parity:.2f}; "
        "a quoted 2.25 is too high"
    )
    r = p.american_call_minus_put(19.0, 20.0, 0.10, 5.0 / 12.0)
    print(f"American: {r.lower:.2f} <= C - P <= {r.upper:.2f}")
    for dividend in (0.5, 1.0):
        may = p.call_early_exercise_may_be_optimal(dividend, 40.0, 0.09, 0.25)
        verdict = "may be optimal" if may else "never optimal"
        print(
            f"Dividend {dividend:.2f} in 3 months, K=40, r=9%: "
            f"early exercise {verdict}"
        )


if __name__ == "__main__":
    main()
