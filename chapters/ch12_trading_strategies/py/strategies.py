"""Chapter 12: spreads, combinations, principal protection, replication.

Run with `python chapters/ch12_trading_strategies/py/strategies.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
Pass --plot to draw the profit diagrams (needs matplotlib).
"""

import argparse
import importlib
from typing import Any

_SPOTS = [20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0]


def _gallery(lib: Any) -> dict[str, Any]:
    st = lib.strategies
    return {
        "Bull call 30/35": st.bull_call_spread(30, 3, 35, 1),
        "Bear put 30/35": st.bear_put_spread(30, 1, 35, 3),
        "Butterfly 55/60/65": st.butterfly_call_spread(55, 10, 60, 7, 65, 5),
        "Straddle 70": st.straddle(70, 4, 3),
        "Strangle 40/50": st.strangle(40, 2, 50, 2),
        "Strip 50": st.strip(50, 3, 2),
        "Strap 50": st.strap(50, 3, 2),
    }


def _plot(gallery: dict[str, Any]) -> None:
    from matplotlib import pyplot  # noqa: PLC0415

    xs = [10.0 + 0.5 * i for i in range(161)]
    _, axes = pyplot.subplots(2, 4, figsize=(14, 6), sharex=True)
    for ax, (name, p) in zip(axes.flat, gallery.items(), strict=False):
        ax.plot(xs, [p.profit_at(x) for x in xs])
        ax.axhline(0.0, linewidth=0.5, color="grey")
        ax.set_title(name)
    pyplot.tight_layout()
    pyplot.show()


def main() -> None:
    """Prints profit tables for the Chapter 12 strategies."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    parser.add_argument("--plot", action="store_true", help="draw diagrams")
    args = parser.parse_args()
    lib = importlib.import_module("ofod_cpp" if args.cpp else "ofod")
    gallery = _gallery(lib)
    print(f"{'Strategy':<22} {'':>11} | profit at S_T = 20, 30, ..., 80")
    for name, p in gallery.items():
        cells = " ".join(f"{p.profit_at(s):>6.2f}" for s in _SPOTS)
        print(f"{name:<22} cost {p.cost():>6.2f} | {cells}")
    budget = lib.strategies.principal_protected_budget(1000.0, 0.06, 3.0)
    print(
        "Principal protected: 1000 at 6% for 3y leaves "
        f"{budget:.2f} for options"
    )
    tent = lib.strategies.replicate_payoff(
        0.0, [0.0, 1.0, -1.0, 0.0], [40.0, 50.0, 60.0]
    )
    print(
        "Replicated tent payoff (kinks 40/50/60), legs: "
        f"{len(tent.legs)}, payoff at 50 = {tent.payoff_at(50.0):g}"
    )
    if args.plot:
        _plot(gallery)


if __name__ == "__main__":
    main()
