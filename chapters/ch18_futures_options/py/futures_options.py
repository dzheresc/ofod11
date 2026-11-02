"""Chapter 18: Black's model, futures option parity, trees, futures-style.

Run with `python chapters/ch18_futures_options/py/futures_options.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math


def main() -> None:
    """Reproduces the Chapter 18 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers
    ins = lib.instruments
    t = 4 / 12
    put = p.black76_price(ins.OptionType.PUT, 20, 20, 0.09, 0.25, t)
    call = p.black76_price(ins.OptionType.CALL, 20, 20, 0.09, 0.25, t)
    print(f"Crude oil futures put (Black): {put:.2f}")
    parity = p.futures_put_from_parity(call, 20, 20, 0.09, t)
    print(f"Parity: put from call {parity:.4f} vs Black {put:.4f}")
    futures = p.TreeMarket(20, 0.09, 0.09, 0.25)
    prices = [
        p.price_binomial(
            ins.VanillaOption(ins.OptionType.PUT, ex, 20, t), futures, 1000
        ).price
        for ex in (ins.Exercise.EUROPEAN, ins.Exercise.AMERICAN)
    ]
    print(
        f"Tree with q = r: European {prices[0]:.4f}, American {prices[1]:.4f}"
    )
    spot = 20.0 * math.exp(-0.09 * t)
    bsm = p.bsm_price(ins.OptionType.CALL, spot, 20, 0.09, 0.0, 0.25, t)
    print(f"Black on F = S e^(rT) equals BSM on S: {call:.6f} vs {bsm:.6f}")
    fs = p.futures_style_option_price(ins.OptionType.CALL, 20, 20, 0.25, t)
    print(f"Futures-style call (no discounting): {fs:.4f}")


if __name__ == "__main__":
    main()
