"""Chapter 6: Treasury quotes, conversion factors, CTD, rate futures.

Run with `python chapters/ch06_interest_rate_futures/py/rate_futures.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import datetime
import importlib


def main() -> None:
    """Reproduces the Chapter 6 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    f = lib.futures
    quoted = f.parse_thirty_seconds("95-16")
    accrued = f.accrued_interest(
        datetime.date(2026, 1, 10),
        datetime.date(2026, 7, 10),
        datetime.date(2026, 3, 5),
        5.5,
    )
    print(
        f"95-16 = {quoted:.4f}, accrued {accrued:.4f}, "
        f"cash {f.cash_price(quoted, accrued):.4f}"
    )
    print(
        "Conversion factors: "
        f"10% 20y2m {f.conversion_factor(0.10, 20 + 2 / 12):.4f}, "
        f"8% 18y4m {f.conversion_factor(0.08, 18 + 4 / 12):.4f}"
    )
    bonds = [
        f.DeliverableBond(99.50, 1.0382),
        f.DeliverableBond(143.50, 1.5188),
        f.DeliverableBond(119.75, 1.2615),
    ]
    ctd = f.find_cheapest_to_deliver(93.25, bonds)
    print(f"Cheapest to deliver: bond {ctd.index + 1} (cost {ctd.cost:.2f})")
    adj = 100.0 * f.convexity_adjustment(0.012, 8.0, 8.25)
    print(f"Convexity adjustment, sigma 1.2%, 8y: {adj:.4f}%")
    n = f.duration_hedge_contracts(10_000_000.0, 6.8, 93_062.5, 9.2)
    print(f"Duration hedge: {n:.2f} contracts")


if __name__ == "__main__":
    main()
