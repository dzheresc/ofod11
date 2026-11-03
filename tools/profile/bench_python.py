#!/usr/bin/env python3
"""Times the same calls in the pure-Python ofod and the ofod_cpp bindings.

Both packages expose the same API, so each case is a function of the
library module. Run from the repository root with both packages installed:

    python tools/profile/bench_python.py --out docs/perf/python.json

Each case is timed with timeit's autorange (at least 0.2 s per repeat) and
the best of --repeat runs is reported, the usual way to filter host noise.
"""

import argparse
from collections.abc import Callable
import json
import platform
import sys
import timeit
import types
from typing import Any

import ofod
import ofod_cpp

Case = Callable[[types.ModuleType], Callable[[], Any]]


def _american_put(lib: types.ModuleType) -> Any:
    ins = lib.instruments
    return ins.VanillaOption(
        ins.OptionType.PUT, ins.Exercise.AMERICAN, 50.0, 5.0 / 12.0
    )


def _market(lib: types.ModuleType) -> Any:
    return lib.pricers.TreeMarket(50.0, 0.10, 0.0, 0.40)


def _bsm(lib: types.ModuleType) -> Callable[[], Any]:
    p, call = lib.pricers, lib.instruments.OptionType.CALL
    return lambda: p.bsm_price(call, 42.0, 40.0, 0.1, 0.0, 0.2, 0.5)


def _implied_vol(lib: types.ModuleType) -> Callable[[], Any]:
    p, call = lib.pricers, lib.instruments.OptionType.CALL
    return lambda: p.implied_volatility(call, 1.875, 21, 20, 0.1, 0.0, 0.25)


def _crr(steps: int) -> Case:
    def make(lib: types.ModuleType) -> Callable[[], Any]:
        put, market = _american_put(lib), _market(lib)
        return lambda: lib.pricers.price_binomial(put, market, steps)

    return make


def _trinomial(lib: types.ModuleType) -> Callable[[], Any]:
    put, market = _american_put(lib), _market(lib)
    return lambda: lib.pricers.price_trinomial(put, market, 500)


def _fd(solver: str) -> Case:
    def make(lib: types.ModuleType) -> Callable[[], Any]:
        p = lib.pricers
        put, market = _american_put(lib), _market(lib)
        grid = p.FdGrid(s_max=200.0, price_steps=200, time_steps=100)
        scheme = p.FdScheme.CRANK_NICOLSON
        american = getattr(p.AmericanSolver, solver)
        return lambda: p.price_finite_difference(
            put, market, grid, scheme, american
        )

    return make


def _mc_european(lib: types.ModuleType) -> Callable[[], Any]:
    p = lib.pricers
    call = lib.instruments.VanillaOption(strike=40.0, maturity=0.5)
    market = _market(lib)
    opts = p.McOptions(paths=100_000)
    return lambda: p.monte_carlo_european(call, market, opts)


def _heston(lib: types.ModuleType) -> Callable[[], Any]:
    p = lib.pricers
    params = p.HestonParams(v0=0.04, kappa=2.0, theta=0.04, sigma=0.3, rho=-0.7)
    market = p.TreeMarket(100.0, 0.03, 0.0, 0.0)
    call = lib.instruments.OptionType.CALL
    return lambda: p.heston_price(call, market, 100.0, 1.0, params)


def _lsm(lib: types.ModuleType) -> Callable[[], Any]:
    p = lib.pricers
    market = p.TreeMarket(36.0, 0.06, 0.0, 0.2)
    put = lib.instruments.OptionType.PUT
    opts = p.LsmOptions(paths=10_000)
    return lambda: p.longstaff_schwartz(put, market, 40.0, 1.0, opts)


def _hull_white_bermudan(lib: types.ModuleType) -> Callable[[], Any]:
    p = lib.pricers
    curve = lib.curves.ZeroCurve.flat(0.04)
    steps = 500
    exercise = list(range(steps // 10, steps, steps // 10))
    put = lib.instruments.OptionType.PUT

    def run() -> Any:
        tree = p.ShortRateTree(curve, 0.1, 0.01, 10.0 / steps, steps)
        return p.tree_swaption(tree, put, 0.04, steps // 10, steps, exercise)

    return run


def _tranche(lib: types.ModuleType) -> Callable[[], Any]:
    c = lib.credit
    mezz = c.Tranche(attachment=0.03, detachment=0.06)
    pool, terms = c.CdoPool(), c.CdoTerms()
    curve = lib.curves.ZeroCurve.flat(0.05)
    return lambda: c.tranche_spread(mezz, pool, 0.3, terms, curve)


def _garch(lib: types.ModuleType) -> Callable[[], Any]:
    returns = [0.01 * ((i * 7919) % 201 - 100) / 100.0 for i in range(1000)]
    return lambda: lib.stats.fit_garch11(returns)


def _project(lib: types.ModuleType) -> Callable[[], Any]:
    p = lib.pricers
    lattice = p.ProjectLattice(
        value=100.0,
        volatility=0.3,
        rate=0.05,
        payout=0.02,
        maturity=5.0,
        steps=500,
    )
    options = [p.Abandon(salvage=80.0), p.Expand(factor=1.3, cost=20.0)]
    return lambda: p.project_with_options(lattice, options)


CASES: dict[str, Case] = {
    "bsm_price": _bsm,
    "implied_volatility": _implied_vol,
    "crr_american_100": _crr(100),
    "crr_american_1000": _crr(1000),
    "trinomial_american_500": _trinomial,
    "fd_cn_american_psor_200x100": _fd("PSOR"),
    "fd_cn_american_brennan_schwartz_200x100": _fd("BRENNAN_SCHWARTZ"),
    "mc_european_100k": _mc_european,
    "heston_price_1y": _heston,
    "longstaff_schwartz_10k": _lsm,
    "hull_white_bermudan_500": _hull_white_bermudan,
    "tranche_spread_125": _tranche,
    "fit_garch11_1000": _garch,
    "project_with_options_500": _project,
}


def _time(fn: Callable[[], Any], repeat: int) -> float:
    timer = timeit.Timer(fn)
    number, _ = timer.autorange()
    number = max(number, 1)
    while True:
        best = min(timer.repeat(repeat=1, number=number)) / number
        if best * number >= 0.2 or number > 1_000_000:
            break
        number *= 2
    runs = timer.repeat(repeat=repeat, number=number)
    return min(runs) / number


def _format(seconds: float) -> str:
    for unit, scale in (("s", 1.0), ("ms", 1e-3), ("us", 1e-6)):
        if seconds >= scale:
            return f"{seconds / scale:.3g} {unit}"
    return f"{seconds / 1e-9:.3g} ns"


def main() -> int:
    """Runs the selected cases and prints a Markdown table."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", help="write results as JSON to this path")
    parser.add_argument("--filter", default="", help="substring of case names")
    parser.add_argument("--repeat", type=int, default=3)
    args = parser.parse_args()

    results = []
    print("| Case | Python | C++ (bindings) | Speed-up |")
    print("| --- | --- | --- | --- |")
    for name, case in CASES.items():
        if args.filter not in name:
            continue
        py = _time(case(ofod), args.repeat)
        cpp = _time(case(ofod_cpp), args.repeat)
        results.append({"name": name, "python_s": py, "cpp_s": cpp})
        print(
            f"| {name} | {_format(py)} | {_format(cpp)} | {py / cpp:.1f}x |",
            flush=True,
        )
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(
                {
                    "python": sys.version.split()[0],
                    "machine": platform.machine(),
                    "results": results,
                },
                f,
                indent=2,
            )
            f.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
