"""Chapter 27: more on models and numerical procedures.

Run with `python chapters/ch27_more_models/py/models.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math


def main() -> None:
    """Reproduces the Chapter 27 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p, ins = lib.pricers, lib.instruments
    call, put = ins.OptionType.CALL, ins.OptionType.PUT
    tm = p.TreeMarket

    def implied(price: float, k: float) -> float:
        otype = put if k < 100.0 else call
        return p.implied_volatility(otype, price, 100.0, k, 0.05, 0.0, 1.0)

    cev = tm(100.0, 0.05, 0.0, 0.25 * 10.0)
    jump = tm(100.0, 0.05, 0.0, 0.2)
    jumps = p.JumpParams(0.5, -0.1, 0.15)
    heston = tm(100.0, 0.05, 0.0, 0.0)
    hp = p.HestonParams(0.0625, 2.0, 0.0625, 0.5, -0.7)
    print("Implied volatility smiles (S = 100, r = 5%, T = 1)")
    print("  strike   CEV a=0.5   Merton jump   Heston")
    for k in (70.0, 80.0, 90.0, 100.0, 110.0, 120.0, 130.0):
        otype = put if k < 100.0 else call
        c = p.cev_price(otype, cev, k, 1.0, 0.5)
        j = p.merton_jump_price(otype, jump, k, 1.0, jumps)
        h = p.heston_price(otype, heston, k, 1.0, hp)
        print(
            f"  {k:6.0f}  {100 * implied(c, k):9.2f}%  "
            f"{100 * implied(j, k):11.2f}%  {100 * implied(h, k):6.2f}%"
        )
    exact = p.heston_price(call, heston, 100.0, 1.0, hp)
    mc = p.heston_monte_carlo(call, heston, 100.0, 1.0, hp, 100, 50_000, 1)
    print(
        f"Heston ATM call: semi-analytic {exact:.4f}, Monte Carlo "
        f"{mc.price:.4f} (se {mc.std_error:.4f})"
    )

    m = tm(36.0, 0.06, 0.0, 0.2)
    option = ins.VanillaOption(put, ins.Exercise.AMERICAN, 40.0, 1.0)
    tree = p.price_binomial(option, m, 2000, p.TreeScheme.COX_ROSS_RUBINSTEIN)
    lsm = p.longstaff_schwartz(put, m, 40.0, 1.0, p.LsmOptions(paths=50_000))
    european = p.bsm_price(put, 36.0, 40.0, 0.06, 0.0, 0.2, 1.0)
    print(
        "American put, S = 36, K = 40 (Longstaff-Schwartz 2001): tree "
        f"{tree.price:.4f}, LSM {lsm.price:.4f} (se {lsm.std_error:.4f}), "
        f"European {european:.4f}"
    )

    m = tm(50.0, 0.05, 0.0, 0.3)
    print("Convertible (Example 27.1): 9 months, 2 shares, callable at 113")
    for steps in (3, 30, 300):
        terms = p.ConvertibleTerms(100.0, 2.0, 0.75, 113.0, 0.4, 0.01)
        v = p.convertible_bond_price(m, terms, steps)
        print(f"  {steps:3d} steps: {v:.4f}")
    terms = p.ConvertibleTerms(100.0, 2.0, 0.75, math.inf, 0.4, 0.01)
    v = p.convertible_bond_price(m, terms, 300)
    print(f"  non-callable, 300 steps: {v:.4f}")


if __name__ == "__main__":
    main()
