"""Chapter 34: swaps revisited.

Run with `python chapters/ch34_swaps_revisited/py/swaps.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
from typing import Any


def _upward(lib: Any) -> Any:
    return lib.curves.ZeroCurve(
        [0.25, 1.0, 2.0, 5.0, 10.0], [0.03, 0.035, 0.04, 0.045, 0.05]
    )


def _variations(lib: Any, curve: Any) -> None:
    p = lib.pricers
    plain = p.swap_schedule(0.0, 5.0, 2, 100.0, 0.0)
    amortizing = p.swap_schedule(0.0, 5.0, 2, 100.0, 0.0, 10.0)
    forward = p.swap_schedule(2.0, 7.0, 2, 100.0, 0.0)
    print(
        "Par fixed rates, 5-year semiannual: plain "
        f"{100 * p.par_fixed_rate(curve, plain):.4f}%, amortizing by 10 a "
        f"period {100 * p.par_fixed_rate(curve, amortizing):.4f}%, starting "
        f"in 2 years {100 * p.par_fixed_rate(curve, forward):.4f}%"
    )
    step_up = p.swap_schedule(0.0, 5.0, 2, 100.0, 0.04, 0.0, 0.001)
    print(
        "Step-up swap, receive 4% rising 10 bp a period on 100: value "
        f"{p.schedule_swap_value(curve, step_up):.4f}"
    )


def _adjusted(lib: Any, curve: Any) -> None:
    p = lib.pricers
    periods = p.swap_schedule(0.0, 5.0, 2, 100.0, 0.045)
    vols = [0.2] * len(periods)
    print(
        "Receive 4.5% on 100 for 5 years: standard "
        f"{p.schedule_swap_value(curve, periods):.4f}, LIBOR-in-arrears "
        f"(20% vol) {p.in_arrears_swap_value(curve, periods, vols):.4f}"
    )
    f = p.simple_forward_rate(curve, 3.0, 3.5)
    print(
        "  in-arrears adjustment for the rate reset at 3 years: "
        f"{100 * f:.4f}% -> {100 * p.in_arrears_rate(f, 0.2, 3.0, 0.5):.4f}%"
    )
    leg = p.CmsLeg(5.0, 2, 0.18, 0.7)
    swap = p.forward_swap_rate(curve, 3.0, 5.0, 2)
    cms = p.cms_rate(
        p.CmsTerms(
            swap.rate,
            0.2,
            3.0,
            5.0,
            2,
            0.5,
            p.simple_forward_rate(curve, 3.0, 3.5),
            0.18,
            0.7,
        )
    )
    print(
        "CMS swap, receive 4.5% against the 5-year swap rate: "
        f"{p.cms_swap_value(curve, periods, leg, vols):.4f}; the rate set at "
        f"3 years is {100 * swap.rate:.4f}% forward, {100 * cms:.4f}% adjusted"
    )


def _diff_and_accrual(lib: Any, curve: Any) -> None:
    p = lib.pricers
    line = "Diff swap: quanto-adjusted foreign forwards (%):"
    for t in (1.0, 2.0, 5.0):
        rate = 100 * 0.04 * p.quanto_adjustment_factor(0.3, 0.1, 0.2, t)
        line += f" {t:.0f}y {rate:.4f}"
    print(line)
    periods = p.swap_schedule(0.0, 3.0, 4, 100.0, 0.05)
    full = 0.0
    for q in periods:
        full += q.notional * q.fixed_rate * 0.25 * curve.discount_factor(q.end)
    leg = p.accrual_fixed_leg_value(curve, periods, 0.03, 0.05, 0.2, 30)
    print(
        "Accrual swap, 5% accruing while the 3-month rate is in [3%, 5%] "
        f"(20% vol, 30 fixings a quarter): fixed leg {leg:.4f} of {full:.4f}"
    )


def _cancelable(lib: Any, curve: Any) -> None:
    p = lib.pricers
    call = lib.instruments.OptionType.CALL
    tree = p.ShortRateTree(curve, 0.1, 0.01, 0.05, 100)
    periods = p.swap_schedule(0.0, 5.0, 1, 1.0, 0.05)
    swap = 100.0 * p.schedule_swap_value(curve, periods)
    bermudan = 100.0 * p.tree_swaption(
        tree, call, 0.05, 20, 100, [20, 40, 60, 80]
    )
    european = 100.0 * p.tree_swaption(tree, call, 0.05, 20, 100, [20])
    print(
        f"Cancelable swap on 100: receive-fixed swap {swap:.4f} + Bermudan "
        f"payer swaption {bermudan:.4f} = {swap + bermudan:.4f} (cancel only "
        f"at year 1: {swap + european:.4f})"
    )


def main() -> None:
    """Reproduces the Chapter 34 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    curve = _upward(lib)
    _variations(lib, curve)
    _adjusted(lib, curve)
    _diff_and_accrual(lib, curve)
    _cancelable(lib, curve)


if __name__ == "__main__":
    main()
