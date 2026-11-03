"""Chapter 36: real options.

Run with `python chapters/ch36_real_options/py/real_options.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math
from typing import Any


def _risk_neutral(lib: Any) -> None:
    p = lib.pricers
    lam = p.market_price_of_risk(0.7, 0.10, 0.04, 0.20)
    print(
        "Market price of risk, rho = 0.7, market excess 6%, market vol 20%: "
        f"{lam:.4f}"
    )
    print(
        "  sales growing at 8% with 30% vol grow at "
        f"{p.risk_neutral_growth(0.08, lam, 0.3):.4f} in the risk-neutral world"
    )
    times = [1.0, 2.0, 3.0]
    expected = [100.0, 110.0, 120.0]
    beta_rate = 0.04 + 0.7 * 0.3 / 0.2 * 0.06
    capm = sum(
        e * math.exp(-beta_rate * t)
        for t, e in zip(times, expected, strict=True)
    )
    rn = p.risk_neutral_present_value(times, expected, lam, 0.3, 0.04)
    print(
        f"  PV of expected flows 100, 110, 120: risk-neutral {rn:.4f}, at the "
        f"CAPM rate {100 * beta_rate:.4f}%: {capm:.4f}"
    )


def _business(lib: Any) -> None:
    p = lib.pricers
    print(
        "Business: revenue 50 growing at 3% (risk-neutral), costs 60% + 18, "
        "cash 5, 8x terminal multiple"
    )
    for vol in (0.0, 0.2, 0.4, 0.6):
        spec = p.BusinessSpec(
            50.0, 0.03, vol, 0.6, 18.0, 5.0, 0.04, 5.0, 4, 8.0
        )
        v = p.value_business(spec, 20_000, 7)
        print(
            f"  revenue vol {100 * vol:3.0f}%: value {v.value:8.4f} (se "
            f"{v.std_error:.4f}), P(bankrupt) {v.bankruptcy_probability:.4f}"
        )


def _lattice(lib: Any) -> None:
    p = lib.pricers

    def lattice(vol: float = 0.3) -> Any:
        return p.ProjectLattice(100.0, vol, 0.05, 0.04, 2.0, 200)

    abandon = [p.Abandon(90.0)]
    values = [
        p.project_with_options(lattice(), opts)
        for opts in (
            abandon,
            [p.Expand(1.3, 25.0)],
            [p.Contract(0.7, 25.0)],
            [p.Abandon(90.0), p.Expand(1.3, 25.0), p.Contract(0.7, 25.0)],
        )
    ]
    print(
        "Project worth 100, 30% vol, 4% payout, options for 2 years (200 "
        "steps):"
    )
    print(
        f"  abandon for 90: {values[0]:.4f}; expand 30% for 25: "
        f"{values[1]:.4f}; contract 30% saving 25: {values[2]:.4f}; all "
        f"three: {values[3]:.4f}"
    )
    print(
        f"  invest 105 now: NPV {100.0 - 105.0:.4f}; option to defer up to 2 "
        f"years: {p.deferral_option(lattice(), 105.0):.4f}"
    )
    by_vol = "".join(
        f" {100 * vol:.0f}%: "
        f"{p.project_with_options(lattice(vol), abandon) - 100.0:.4f}"
        for vol in (0.1, 0.2, 0.3, 0.4, 0.5)
    )
    print(f"  value of the abandonment option by volatility:{by_vol}")


def _extraction(lib: Any) -> None:
    p = lib.pricers
    tree = p.CommodityTree(20.0, [22.0, 23.0, 24.0], 0.1, 0.2, 1.0)
    v = p.value_extraction_project(
        tree, p.ExtractionProject(2.0, 17.0, 6.0, 0.1, 0.0, 1.2, 2.0)
    )
    print(
        "Resource project on the Ch 35 tree, 2 units a year for 3 years, "
        "variable cost 17, fixed 6:"
    )
    print(
        f"  base {v.base:.4f}; with abandonment {v.with_abandon:.4f} (option "
        f"{v.with_abandon - v.base:.4f}); with 20% expansion for 2 "
        f"{v.with_expand:.4f} (option {v.with_expand - v.base:.4f})"
    )


def main() -> None:
    """Runs the chapter's examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    _risk_neutral(lib)
    _business(lib)
    _lattice(lib)
    _extraction(lib)


if __name__ == "__main__":
    main()
