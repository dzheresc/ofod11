"""Chapter 32: no-arbitrage short-rate models and trinomial trees.

Run with `python chapters/ch32_no_arbitrage_models/py/trees.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
from typing import Any


def _hull_curve(lib: Any) -> Any:
    return lib.curves.ZeroCurve(
        [0.5, 1.0, 1.5, 2.0, 2.5, 3.0],
        [0.0343, 0.03824, 0.04183, 0.04512, 0.04812, 0.05086],
    )


def _hull_tree(lib: Any, curve: Any) -> None:
    tree = lib.pricers.ShortRateTree(curve, 0.1, 0.01, 1.0, 3)
    print(
        f"Hull-White tree, a = 0.1, sigma = 0.01, dt = 1: dx = {tree.dx:.5f}, "
        f"jmax = {tree.jmax}"
    )
    print("  alphas:" + "".join(f" {100 * a:.3f}%" for a in tree.alphas))
    print("  node   pu      pm      pd")
    for j in range(2, -3, -1):
        b = tree.branch_at(j)
        print(f"  {j:+d}   {b.up:.4f}  {b.mid:.4f}  {b.down:.4f}")
    rates = "".join(f" {100 * tree.rate(2, j):.3f}" for j in range(-2, 3))
    print("  rates at step 2 (%):" + rates)


def _convergence(lib: Any, curve: Any, hw: Any) -> None:
    p = lib.pricers
    ot = lib.instruments.OptionType
    ex = lib.instruments.Exercise
    analytic = hw.zero_bond_option(ot.PUT, 1.0, 3.0, 0.92)
    print(f"1-year put on a 3-year zero, K = 0.92: analytic {analytic:.6f}")
    print("  dt      tree       error")
    for n in (4, 10, 20, 50, 100):
        dt = 1.0 / n
        tree = p.ShortRateTree(curve, 0.1, 0.01, dt, 3 * n)
        zero = [p.TreeCashFlow(3 * n, 1.0)]
        v = p.tree_bond_option(tree, ot.PUT, ex.EUROPEAN, n, zero, 0.92)
        print(f"  {dt:.3f}  {v:.6f}  {v - analytic:+.6f}")
    jamshidian = hw.coupon_bond_option(
        ot.CALL, 1.0, [2.0, 3.0], [5.0, 105.0], 99.0
    )
    tree = p.ShortRateTree(curve, 0.1, 0.01, 0.01, 300)
    bond = [p.TreeCashFlow(200, 5.0), p.TreeCashFlow(300, 105.0)]
    euro = p.tree_bond_option(tree, ot.CALL, ex.EUROPEAN, 100, bond, 99.0)
    amer = p.tree_bond_option(tree, ot.CALL, ex.AMERICAN, 100, bond, 99.0)
    print(
        f"Call on a 3-year 5% bond, K = 99, in 1 year: Jamshidian "
        f"{jamshidian:.4f}, tree (dt = 0.01) {euro:.4f}, American on the tree "
        f"{amer:.4f}"
    )


def _swaptions(lib: Any, curve: Any) -> None:
    p = lib.pricers
    put = lib.instruments.OptionType.PUT
    print("Receiver swaptions, annual 5% swap ending in 3 years, dt = 0.02")
    print("  model                 European  Bermudan")
    for name, model, sigma in (
        ("Hull-White 1%", p.ShortRateModel.HULL_WHITE, 0.01),
        ("Black-Karasinski 25%", p.ShortRateModel.BLACK_KARASINSKI, 0.25),
    ):
        tree = p.ShortRateTree(curve, 0.1, sigma, 0.02, 150, model)
        euro = p.tree_swaption(tree, put, 0.05, 50, 150, [50])
        berm = p.tree_swaption(tree, put, 0.05, 50, 150, [50, 100])
        print(f"  {name:20}  {euro:8.5f}  {berm:8.5f}")


def _calibration(lib: Any, curve: Any) -> None:
    p = lib.pricers
    md = lib.models
    call = lib.instruments.OptionType.CALL
    quotes = []
    for reset in (0.5, 1.0, 1.5, 2.0, 2.5):
        forward = p.simple_forward_rate(curve, reset, reset + 0.5)
        for strike in (0.04, 0.06):
            price = p.black_caplet_price(
                call,
                forward,
                strike,
                0.32 - 0.06 * reset,
                reset,
                0.5,
                curve.discount_factor(reset + 0.5),
                1.0,
            )
            quotes.append(md.CapletQuote(reset, 0.5, strike, price))
    fit = md.calibrate_hull_white(curve, quotes)
    print(
        f"Hull-White calibrated to 10 caplets with falling Black vols: a = "
        f"{fit.a:.4f}, sigma = {100 * fit.sigma:.4f}%, RMS relative error "
        f"{100 * fit.rms_relative_error:.2f}%"
    )


def main() -> None:
    """Reproduces the Chapter 32 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    curve = _hull_curve(lib)
    hw = lib.models.HullWhite(curve, 0.1, 0.01)
    _hull_tree(lib, curve)
    _convergence(lib, curve, hw)
    _swaptions(lib, curve)
    _calibration(lib, curve)


if __name__ == "__main__":
    main()
