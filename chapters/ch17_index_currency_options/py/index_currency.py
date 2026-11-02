"""Chapter 17: index and currency options, portfolio insurance, trees.

Run with `python chapters/ch17_index_currency_options/py/index_currency.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces the Chapter 17 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers
    ins = lib.instruments
    call = ins.OptionType.CALL
    c = p.bsm_price(call, 930, 900, 0.08, 0.03, 0.20, 2 / 12)
    print(f"Example 17.1 index call: {c:.2f}")
    c = p.garman_kohlhagen_price(call, 1.6, 1.6, 0.08, 0.11, 0.141, 4 / 12)
    print(f"GBP call (S=1.6, K=1.6, r=8%, rf=11%, sigma=14.1%, 4m): {c:.4f}")
    n = lib.risk.beta_hedge_contracts(2.0, 500_000, 1000.0 * 100)
    print(
        "Portfolio insurance: beta 2.0, $500,000, index 1000 x 100 -> "
        f"{n:.0f} put contracts"
    )
    fx = p.TreeMarket(1.6, 0.08, 0.11, 0.141)
    prices = [
        p.price_binomial(ins.VanillaOption(call, ex, 1.6, 1.0), fx, 500).price
        for ex in (ins.Exercise.EUROPEAN, ins.Exercise.AMERICAN)
    ]
    print(
        f"1y GBP call on a tree: European {prices[0]:.4f}, American "
        f"{prices[1]:.4f} (rf > r makes early exercise valuable)"
    )


if __name__ == "__main__":
    main()
