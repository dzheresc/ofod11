"""Chapter 2: daily settlement of a long gold futures position (Table 2.1).

Run with `python chapters/ch02_futures_markets/py/margin_account.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib

_PRICES = [
    1250.0, 1241.0, 1238.3, 1244.6, 1241.3, 1240.1, 1236.2, 1229.9, 1230.8,
    1225.4, 1228.1, 1211.0, 1211.0, 1214.3, 1216.1, 1223.0, 1226.9,
]  # fmt: skip


def main() -> None:
    """Prints the margin account table."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    spec = lib.futures.MarginSpec(
        initial_margin=6000.0,
        maintenance_margin=4500.0,
        contract_size=100.0,
        contracts=2,
    )
    print("Day   Price   Daily   Cumul.  Balance  Call")
    days = lib.futures.simulate_margin_account(spec, _PRICES)
    for n, d in enumerate(days):
        print(
            f"{n:>3} {d.price:>8.2f} {d.daily_gain:>7.0f} "
            f"{d.cumulative_gain:>8.0f} {d.balance:>8.0f} {d.margin_call:>5.0f}"
        )


if __name__ == "__main__":
    main()
