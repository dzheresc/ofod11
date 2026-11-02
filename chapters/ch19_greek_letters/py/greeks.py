"""Chapter 19: the Greek letters.

Run with `python chapters/ch19_greek_letters/py/greeks.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
from typing import Any

_STRIKE = 50.0
_MATURITY = 20.0 / 52.0


def main() -> None:
    """Reproduces the Chapter 19 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    g = lib.greeks
    call_type = lib.instruments.OptionType.CALL

    def call(x: Any) -> float:
        return lib.pricers.bsm_price(
            call_type, x.spot, _STRIKE, x.rate, x.yield_, x.volatility,
            x.maturity,
        )  # fmt: skip

    x = g.OptionInputs(
        spot=49.0, rate=0.05, yield_=0.0, volatility=0.20, maturity=_MATURITY
    )
    a = g.bsm_greeks(call_type, _STRIKE, x)
    b = g.bump_greeks(call, x)
    print(
        "Call S0 = 49, K = 50, r = 5%, sigma = 20%, 20 weeks: price "
        f"{call(x):.4f}"
    )
    print("            analytic     bumped   Hull's units")
    print(f"  delta  {a.delta:>11.6f} {b.delta:>10.6f}   {a.delta:.3f}")
    print(f"  gamma  {a.gamma:>11.6f} {b.gamma:>10.6f}   {a.gamma:.3f}")
    print(
        f"  vega   {a.vega:>11.6f} {b.vega:>10.6f}   {a.vega / 100:.3f} "
        "per 1% vol"
    )
    print(
        f"  theta  {a.theta:>11.6f} {b.theta:>10.6f}   "
        f"{a.theta / 365:.4f} per calendar day"
    )
    print(
        f"  rho    {a.rho:>11.6f} {b.rho:>10.6f}   {a.rho / 100:.4f} "
        "per 1% rate"
    )

    h = g.gamma_neutral_hedge(
        g.Exposure(0.0, -3000.0, 0.0), g.Exposure(0.62, 1.5, 0.0)
    )
    print(
        f"Gamma neutral: buy {h.option1:.0f} options, trade "
        f"{h.underlying:.0f} shares"
    )
    h = g.gamma_vega_neutral_hedge(
        g.Exposure(0.0, -5000.0, -8000.0),
        g.Exposure(0.6, 0.5, 2.0),
        g.Exposure(0.5, 0.8, 1.2),
    )
    print(
        f"Gamma and vega neutral: {h.option1:.0f} of option 1, "
        f"{h.option2:.0f} of option 2, trade {h.underlying:.0f} shares"
    )

    spots = [45.0, 47.0, 49.0, 51.0, 53.0]
    vols = [0.15, 0.20, 0.25]
    grid = g.scenario_grid(call, x, spots, vols)
    print("Scenario grid: change in value of one short call")
    print("  vol\\S" + "".join(f"{s:>9.0f}" for s in spots))
    for vol, row in zip(vols, grid, strict=True):
        print(
            f"  {100 * vol:>4.0f}%" + "".join(f"{0.0 - v:>9.4f}" for v in row)
        )

    print(
        "Delta hedging performance (std of cost / BSM price), "
        "mu = 13%, 20,000 paths"
    )
    e = g.HedgeExperiment(49.0, _STRIKE, 0.05, 0.13, 0.20, _MATURITY)
    for n in (4, 5, 10, 20, 40, 80):
        r = g.simulate_delta_hedge(e, n, 20000, 1)
        print(
            f"  rebalance every {20 / n:>4.2f} weeks: {r.ratio:.3f} "
            f"(mean cost {r.mean_cost:.3f})"
        )


if __name__ == "__main__":
    main()
