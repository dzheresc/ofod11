"""Chapter 1: payoffs and profits of the six basic positions.

Run with `python chapters/ch01_introduction/py/payoffs.py [--plot]`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Prints the profit table and optionally plots it."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plot", action="store_true", help="show a chart")
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    args = parser.parse_args()
    lib = importlib.import_module("ofod_cpp" if args.cpp else "ofod")
    ins = lib.instruments

    strike, premium = 100.0, 5.0
    short = ins.Side.SHORT
    positions = {
        "long F": ins.Position(ins.ForwardPayoff(strike)),
        "short F": ins.Position(ins.ForwardPayoff(strike), side=short),
        "long C": ins.Position(ins.CallPayoff(strike), premium=premium),
        "short C": ins.Position(
            ins.CallPayoff(strike), side=short, premium=premium
        ),
        "long P": ins.Position(ins.PutPayoff(strike), premium=premium),
        "short P": ins.Position(
            ins.PutPayoff(strike), side=short, premium=premium
        ),
    }
    spots = [70.0 + 10.0 * i for i in range(7)]
    print(f"Profit at expiry, K = {strike:g}, option premium = {premium:g}")
    print(f"{'S_T':>6} " + " ".join(f"{name:>8}" for name in positions))
    for s in spots:
        row = " ".join(f"{p.profit_at(s):8.1f}" for p in positions.values())
        print(f"{s:6.0f} {row}")

    if args.plot:
        from matplotlib import pyplot as plt  # Optional dependency.

        fine = [60.0 + 0.5 * i for i in range(161)]
        fig, axes = plt.subplots(2, 3, figsize=(10, 6), sharey=True)
        for ax, (name, p) in zip(axes.flat, positions.items(), strict=True):
            ax.plot(fine, [p.profit_at(s) for s in fine])
            ax.axhline(0.0, color="grey", linewidth=0.5)
            ax.set_title(name)
        fig.supxlabel("S_T")
        fig.supylabel("profit")
        plt.tight_layout()
        plt.show()


if __name__ == "__main__":
    main()
