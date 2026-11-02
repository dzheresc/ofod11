"""Chapter 25: credit derivatives.

Run with `python chapters/ch25_credit_derivatives/py/cds.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces the Chapter 25 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    c, cv = lib.credit, lib.curves

    terms = c.CdsTerms(5.0, 1, 0.4)
    credit = cv.CreditCurve.flat(0.02)
    discount = cv.ZeroCurve.flat(0.05)
    legs = c.cds_leg_values(terms, credit, discount)
    spread = c.cds_par_spread(terms, credit, discount)
    buyer = c.cds_buyer_value(terms, 0.015, credit, discount)
    upfront = c.cds_upfront(terms, 0.01, credit, discount)
    print("5-year CDS, hazard 2%, recovery 40%, r = 5%, annual")
    print(
        f"  premium annuity {legs.premium_annuity:.4f}  protection "
        f"{legs.protection:.6f}  par spread {1e4 * spread:.1f} bp"
    )
    print(
        f"  seller of a 150 bp CDS gains {-buyer:.6f}; upfront at a 100 bp "
        f"coupon {upfront:.6f}"
    )

    mats = [1.0, 3.0, 5.0, 7.0, 10.0]
    quotes = [0.005, 0.007, 0.009, 0.010, 0.011]
    curve = c.bootstrap_credit_curve(mats, quotes, 4, 0.4, discount)
    print("Bootstrapped hazards (quarterly CDS, 40% recovery)")
    for m, q, h in zip(mats, quotes, curve.hazards, strict=True):
        s = c.cds_par_spread(c.CdsTerms(m, 4, 0.4), curve, discount)
        print(
            f"  {m:4.0f}y  quote {1e4 * q:5.1f} bp  hazard {h:.6f}  "
            f"repriced {1e4 * s:5.1f} bp"
        )
    option = c.cds_option_price(True, 0.012, 0.01, 0.5, 1.0, 3.5)
    print(
        "Payer CDS option, F = 120 bp, K = 100 bp, vol 50%, 1y, annuity "
        f"3.5: {option:.6f}"
    )

    pool, cdo = c.CdoPool(125, 0.01, 0.4), c.CdoTerms()
    z = cv.ZeroCurve.flat(0.035)
    bounds = [(0.0, 0.03), (0.03, 0.06), (0.06, 0.09), (0.09, 0.12)]
    tranches = [c.Tranche(a, d) for a, d in [*bounds, (0.12, 0.22)]]
    print("CDO tranche spreads (bp), 125 names, hazard 1%, R = 40%")
    print("  tranche    rho 0.05  rho 0.15  rho 0.30  rho 0.60")
    for t in tranches:
        label = f"{100 * t.attachment:.0f}-{100 * t.detachment:.0f}%"
        cells = "".join(
            f"{1e4 * c.tranche_spread(t, pool, rho, cdo, z):10.2f}"
            for rho in (0.05, 0.15, 0.30, 0.60)
        )
        print(f"  {label:<9}{cells}")
    quote = c.tranche_spread(tranches[0], pool, 0.2, cdo, z)
    rho = c.implied_tranche_correlation(tranches[0], pool, quote, cdo, z)
    print(
        f"Equity tranche at {1e4 * quote:.2f} bp implies correlation {rho:.6f}"
    )

    print("nth-to-default, 10 names, hazard 1%, rho 0.3 (50,000 paths)")
    for nth in (1, 2, 3):
        r = c.nth_to_default_spread(c.BasketTerms(nth=nth), 0.05, 50_000, 1)
        print(f"  {nth}: {1e4 * r.spread:.1f} bp (se {1e4 * r.std_error:.1f})")


if __name__ == "__main__":
    main()
