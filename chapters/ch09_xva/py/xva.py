"""Chapter 9: XVAs.

Run with `python chapters/ch09_xva/py/xva.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
from typing import Any


def _print(title: str, p: Any) -> None:
    print(title)
    print("  time      EE     ENE   PFE97.5")
    rows = zip(
        p.times,
        p.expected_exposure,
        p.expected_negative_exposure,
        p.potential_future_exposure,
        strict=True,
    )
    for t, ee, ene, pfe in rows:
        print(f"  {t:4.2f} {ee:7.2f} {ene:7.2f} {pfe:9.2f}")


def main() -> None:
    """Reproduces the Chapter 9 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    c = lib.credit

    times, ee = [1.0, 2.0, 3.0], [10.0, 8.0, 5.0]
    counterparty = lib.curves.CreditCurve.flat(0.02)
    cva = c.cva(times, ee, counterparty, 0.4)
    fva = c.funding_value_adjustment(times, ee, 0.005)
    print(
        f"Exposures (10, 8, 5) at 1-3 years: CVA {cva:.4f}, FVA at 50 bp "
        f"{fva:.4f}"
    )

    call = lib.pricers.bsm_price(
        lib.instruments.OptionType.CALL, 1600.0, 1500.0, 0.05, 0.0, 0.2, 1.0
    )
    forward_cva = c.cva([1.0], [call], counterparty, 0.3)
    print(
        f"Forward, default only at maturity: EE {call:.4f}, CVA "
        f"{forward_cva:.4f}"
    )

    args = (1600.0, 1500.0, 0.05, 0.2, 1.0, 4, 100_000, 3)
    plain = c.simulate_forward_exposure(*args)
    capped = c.simulate_forward_exposure(*args, 0.975, 100.0)
    _print("Simulated forward exposure (discounted, 100,000 paths)", plain)
    _print("With a collateral threshold of 100", capped)
    own = lib.curves.CreditCurve.flat(0.01)
    cva_mc = c.cva(plain.times, plain.expected_exposure, counterparty, 0.3)
    cva_capped = c.cva(
        capped.times, capped.expected_exposure, counterparty, 0.3
    )
    dva = c.dva(plain.times, plain.expected_negative_exposure, own, 0.4)
    print(
        f"Quarterly default: CVA {cva_mc:.4f}, with threshold "
        f"{cva_capped:.4f}, DVA (own hazard 1%) {dva:.4f}"
    )


if __name__ == "__main__":
    main()
