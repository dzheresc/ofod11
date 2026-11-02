"""Chapter 10: payoffs, contract adjustments and naked option margin.

Run with `python chapters/ch10_options_markets/py/options_markets.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces the Chapter 10 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    ins = lib.instruments
    call = ins.VanillaOption(strike=40.0)
    for s in (30.0, 40.0, 50.0):
        payoff = ins.intrinsic_value(call, s)
        print(f"Call K=40 at S_T={s:.0f}: payoff {payoff:.0f}")
    split = ins.adjust_for_split(ins.ContractTerms(30.0, 100.0), 3, 2)
    print(
        f"3-for-2 split: K 30 -> {split.strike:.2f}, {split.shares:.0f} shares"
    )
    div = ins.adjust_for_stock_dividend(ins.ContractTerms(15.0, 100.0), 0.25)
    print(
        f"25% stock dividend: K 15 -> {div.strike:.2f}, {div.shares:.0f} shares"
    )
    margin = ins.naked_option_margin(ins.OptionType.CALL, 5.0, 38.0, 40.0)
    print(
        "Naked calls: 4 contracts, c=5, K=40, S=38 -> "
        f"margin {400 * margin:.0f}"
    )


if __name__ == "__main__":
    main()
