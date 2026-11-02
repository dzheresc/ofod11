"""Chapter 26: exotic options.

Run with `python chapters/ch26_exotic_options/py/exotics.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math


def main() -> None:
    """Reproduces the Chapter 26 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers
    call, put = lib.instruments.OptionType.CALL, lib.instruments.OptionType.PUT
    tm = p.TreeMarket

    gap = p.gap_option_price(
        put, tm(500_000, 0.05, 0.0, 0.2), 400_000, 350_000, 1.0
    )
    print(f"Gap put (Example 26.1): {gap:.2f}")
    m = tm(50.0, 0.08, 0.0, 0.25)
    print(
        "Chooser, S = K = 50, choose at 3 months, expiry 6 months: "
        f"{p.chooser_price(m, 50.0, 0.25, 0.5):.4f} (call "
        f"{p.bsm_price(call, 50, 50, 0.08, 0, 0.25, 0.5):.4f} + put "
        f"{p.bsm_price(put, 50, 50, 0.08, 0, 0.25, 0.5):.4f} bought "
        "separately)"
    )
    fs = p.forward_start_price(call, tm(60.0, 0.05, 0.02, 0.3), 0.5, 1.0)
    print(
        "Forward start call, starts in 6 months, 1-year expiry, q = 2%: "
        f"{fs:.4f}"
    )
    perp = tm(50.0, 0.05, 0.03, 0.3)
    print(
        "Perpetual American, S = K = 50, q = 3%: call "
        f"{p.perpetual_american_price(call, perp, 50.0):.4f}, put "
        f"{p.perpetual_american_price(put, perp, 50.0):.4f}"
    )
    b = tm(100.0, 0.05, 0.02, 0.3)
    print(
        "Binary call, K = 110, 6 months: cash-or-nothing "
        f"{p.cash_or_nothing_price(call, b, 110.0, 0.5):.4f}, "
        f"asset-or-nothing {p.asset_or_nothing_price(call, b, 110.0, 0.5):.4f}"
    )

    m = tm(50.0, 0.05, 0.0, 0.3)
    print("Compound options, K1 = 3 at 3 months on K2 = 50 at 1 year")
    ct = p.CompoundType
    for name, c in (
        ("call on call", ct.CALL_ON_CALL),
        ("put on call", ct.PUT_ON_CALL),
        ("call on put", ct.CALL_ON_PUT),
        ("put on put", ct.PUT_ON_PUT),
    ):
        v = p.compound_option_price(c, m, 3.0, 0.25, 50.0, 1.0)
        print(f"  {name:<12} {v:.4f}")

    m = tm(100.0, 0.08, 0.04, 0.25)
    print("Barrier options, S = K = 100, 6 months (H = 95 down, 105 up)")
    print("  type           call      put")
    bt = p.BarrierType
    for name, kind, h in (
        ("down-and-out", bt.DOWN_AND_OUT, 95.0),
        ("down-and-in", bt.DOWN_AND_IN, 95.0),
        ("up-and-out", bt.UP_AND_OUT, 105.0),
        ("up-and-in", bt.UP_AND_IN, 105.0),
    ):
        c = p.barrier_option_price(kind, call, m, 100.0, h, 0.5)
        q = p.barrier_option_price(kind, put, m, 100.0, h, 0.5)
        print(f"  {name:<12} {c:8.4f} {q:8.4f}")

    look = tm(50.0, 0.1, 0.0, 0.4)
    print(
        "Lookbacks, S0 = 50, sigma = 40%, r = 10%, 3 months: floating call "
        f"{p.floating_lookback_price(call, look, 50.0, 0.25):.4f}, floating "
        f"put {p.floating_lookback_price(put, look, 50.0, 0.25):.4f}, fixed "
        "call K = 50 "
        f"{p.fixed_lookback_price(call, look, 50.0, 50.0, 0.25):.4f}"
    )
    print(
        "Asians (Example 26.3), S0 = K = 50, 1 year: geometric "
        f"{p.geometric_asian_price(call, look, 50.0, 1.0):.4f}, arithmetic "
        "by moment matching "
        f"{p.arithmetic_asian_price(call, look, 50.0, 1.0):.4f}, vanilla "
        f"{p.bsm_price(call, 50, 50, 0.1, 0, 0.4, 1.0):.4f}"
    )

    a = p.ExchangeAssets(100.0, 95.0, 0.02, 0.01, 0.3, 0.2, 0.4)
    print(
        "Exchange V for U in 1 year (Margrabe): "
        f"{p.exchange_option_price(a, 1.0):.4f}"
    )
    strikes = [float(k) for k in range(50, 201)]
    m = tm(100.0, 0.04, 0.01, 0.0)
    flat = p.variance_swap_fair_variance(m, 0.5, strikes, lambda k: 0.25)
    skew = p.variance_swap_fair_variance(
        m, 0.5, strikes, lambda k: 0.25 - 0.1 * math.log(k / 100.0)
    )
    print(
        "Variance swap, 6 months, strikes 50-200: flat 25% vol gives "
        f"{flat:.6f} ({100 * math.sqrt(flat):.2f}% vol); skewed smile gives "
        f"{skew:.6f} ({100 * math.sqrt(skew):.2f}% vol)"
    )


if __name__ == "__main__":
    main()
