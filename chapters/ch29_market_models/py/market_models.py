"""Chapter 29: standard market models for interest rate options.

Run with `python chapters/ch29_market_models/py/market_models.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math
from typing import Any


def _upward(lib: Any) -> Any:
    return lib.curves.ZeroCurve(
        [0.25, 1.0, 2.0, 5.0, 10.0], [0.03, 0.035, 0.04, 0.045, 0.05]
    )


def _book_examples(lib: Any) -> None:
    p = lib.pricers
    call = lib.instruments.OptionType.CALL
    coupons = 50.0 * math.exp(-0.25 * 0.09) + 50.0 * math.exp(-0.75 * 0.095)
    discount = math.exp(-0.1 * 10.0 / 12.0)
    forward = p.forward_bond_price(960.0, coupons, discount)
    price = p.black_bond_option_price(
        call, forward, 1000.0, discount, 0.09, 10.0 / 12.0
    )
    print(
        f"Example 29.1 bond option: coupons PV {coupons:.2f}, forward price "
        f"{forward:.2f}, call {price:.2f}"
    )
    caplet = p.black_caplet_price(
        call, 0.07, 0.08, 0.2, 1.0, 0.25, math.exp(-0.065 * 1.25), 10_000.0
    )
    print(
        "Example 29.2 caplet: L = 10,000, F = 7%, K = 8%, 20% vol: "
        f"{caplet:.2f}"
    )
    swap = p.forward_swap_rate(lib.curves.ZeroCurve.flat(0.06), 5.0, 3.0, 2)
    swaption = p.black_swaption_price(
        call, swap.rate, 0.062, 0.2, 5.0, swap.annuity, 100.0
    )
    print(
        f"Example 29.3 swaption: annuity {swap.annuity:.4f}, forward swap rate "
        f"{100 * swap.rate:.4f}%, payer at 6.2% on $100M {swaption:.4f}M"
    )


def _caps_and_stripping(lib: Any, curve: Any) -> None:
    p = lib.pricers
    ot = lib.instruments.OptionType
    terms = p.CapTerms(0.04, 5.0, 4)
    cap = p.cap_floor_price(ot.CALL, curve, terms, 0.2)
    floor = p.cap_floor_price(ot.PUT, curve, terms, 0.2)
    swap = 0.0
    for k in range(1, 20):
        reset = 0.25 * k
        swap += (
            0.25
            * (p.simple_forward_rate(curve, reset, reset + 0.25) - 0.04)
            * curve.discount_factor(reset + 0.25)
        )
    print(
        "5-year quarterly cap and floor at 4%, 20% vol, per 100: cap "
        f"{100 * cap:.4f}, floor {100 * floor:.4f}, cap - floor "
        f"{100 * (cap - floor):.4f} = swap {100 * swap:.4f}"
    )
    flat = [0.18, 0.2, 0.21, 0.21, 0.2, 0.19, 0.185, 0.18, 0.175]
    spot = p.strip_caplet_volatilities(curve, 0.04, 2, flat)
    print("Caplet vol stripping (semiannual, K = 4%)")
    print("  cap maturity  flat vol  spot vol")
    for i, (f, s) in enumerate(zip(flat, spot, strict=True)):
        print(f"  {0.5 * (i + 2):12.1f}  {100 * f:7.2f}%  {100 * s:7.2f}%")


def _swaption_grid(lib: Any, curve: Any) -> None:
    p = lib.pricers
    call = lib.instruments.OptionType.CALL
    print("ATM payer swaptions per 100 notional, 20% vol, semiannual")
    print("  expiry   2y swap   5y swap  10y swap")
    for expiry in (1.0, 2.0, 5.0):
        row = f"  {expiry:5.0f}y"
        for tenor in (2.0, 5.0, 10.0):
            s = p.forward_swap_rate(curve, expiry, tenor, 2)
            price = p.black_swaption_price(
                call, s.rate, s.rate, 0.2, expiry, s.annuity, 100.0
            )
            row += f"  {price:8.4f}"
        print(row)


def _normal_model(lib: Any) -> None:
    p = lib.pricers
    negative = lib.curves.ZeroCurve([1.0, 10.0], [-0.005, 0.002])
    s = p.forward_swap_rate(negative, 2.0, 5.0, 1)
    price = p.bachelier_swaption_price(
        lib.instruments.OptionType.PUT,
        s.rate,
        0.0,
        0.005,
        2.0,
        s.annuity,
        100.0,
    )
    print(
        f"Normal model: 2y x 5y forward swap rate {100 * s.rate:.4f}%, "
        f"receiver at 0% with 50 bp normal vol {price:.4f} per 100"
    )


def _fmt_time(t: float) -> str:
    # Matches std::format's shortest round-trip output for these pillars.
    return f"{t:g}"


def _hedging(lib: Any, curve: Any) -> None:
    p = lib.pricers
    call = lib.instruments.OptionType.CALL

    def cap(c: Any) -> float:
        return 100.0 * p.cap_floor_price(call, c, p.CapTerms(0.04, 5.0), 0.2)

    buckets = p.bucket_dv01(cap, curve)
    line = "Bucketed DV01 of the cap (per 100, 1 bp):"
    for t, b in zip(curve.times, buckets, strict=True):
        line += f" {_fmt_time(t)}y {b:.5f}"
    line += (
        f"; sum {sum(buckets):.5f}, parallel {p.parallel_dv01(cap, curve):.5f}"
    )
    print(line)


def main() -> None:
    """Reproduces the Chapter 29 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    curve = _upward(lib)
    _book_examples(lib)
    _caps_and_stripping(lib, curve)
    _swaption_grid(lib, curve)
    _normal_model(lib)
    _hedging(lib, curve)


if __name__ == "__main__":
    main()
