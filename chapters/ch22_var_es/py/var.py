"""Chapter 22: value at risk and expected shortfall.

Run with `python chapters/ch22_var_es/py/var.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math
import pathlib

_DATA = pathlib.Path(__file__).resolve().parents[3] / "data" / "market"


def _read_csv(name: str) -> list[list[float]]:
    rows = []
    for line in (_DATA / name).read_text().splitlines():
        if line and not line.startswith("#"):
            rows.append([float(x) for x in line.split(",")])
    return rows


def main() -> None:
    """Reproduces the Chapter 22 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    r, st = lib.risk, lib.stats

    msft = r.scale_to_horizon(r.normal_var(200_000, 0.99), 10)
    att = r.scale_to_horizon(r.normal_var(50_000, 0.99), 10)
    sd = r.portfolio_std_dev([10e6, 5e6], [0.02, 0.01], [[1, 0.3], [0.3, 1]])
    both = r.scale_to_horizon(r.normal_var(sd, 0.99), 10)
    print("Linear model, 10-day 99% VaR")
    print(
        f"  Microsoft {msft:.0f}  AT&T {att:.0f}  together {both:.0f}  "
        f"diversification benefit {msft + att - both:.0f}"
    )
    print(
        f"Normal loss with sd 20: 99% VaR {r.normal_var(20.0, 0.99):.2f}, "
        f"99% ES {r.normal_expected_shortfall(20.0, 0.99):.2f}, 97.5% ES "
        f"{r.normal_expected_shortfall(20.0, 0.975):.2f}"
    )

    u = [row[0] for row in _read_csv("garch_returns.csv")]
    position = 10e6
    losses = [-position * x for x in u]
    fit = st.fit_garch11(u)
    v = list(fit.variances(u))
    vols = [math.sqrt(x) for x in v]
    current = math.sqrt(fit.update(v[-1], u[-1]))
    scaled = r.volatility_adjusted_changes(losses, vols, current)
    print(
        f"Historical simulation, $10M on {len(u)} days of returns (1-day 99%)"
    )
    print(
        f"  plain:             VaR {r.historical_var(losses, 0.99):.0f}  "
        f"ES {r.historical_expected_shortfall(losses, 0.99):.0f}"
    )
    print(
        f"  GARCH vol-scaled:  VaR {r.historical_var(scaled, 0.99):.0f}  "
        f"ES {r.historical_expected_shortfall(scaled, 0.99):.0f}  "
        f"(vol today {100 * current:.2f}%)"
    )
    ewma = st.ewma_variances(u, 0.94, v[1])
    exceptions = sum(
        1
        for i in range(1, len(u))
        if losses[i] > r.normal_var(position * math.sqrt(ewma[i]), 0.99)
    )
    days = len(u) - 1
    print(
        f"Backtest of 99% EWMA VaR: {exceptions} exceptions in {days} days "
        f"(expected {0.01 * days:.1f}), Kupiec "
        f"{r.kupiec_statistic(days, exceptions, 0.01):.2f}, P(>= {exceptions})"
        f" = {r.binomial_tail_probability(days, exceptions, 0.01):.3f}"
    )

    call = lib.instruments.OptionType.CALL
    x = lib.greeks.OptionInputs(50.0, 0.05, 0.0, 0.25, 0.25)
    g = lib.greeks.bsm_greeks(call, 50.0, x)
    moments = r.delta_gamma_moments(-1000 * g.delta, -1000 * g.gamma, 0.8)

    def value(spot: float) -> float:
        return lib.pricers.bsm_price(call, spot, 50.0, 0.05, 0.0, 0.25, 0.25)

    v0 = value(50.0)
    mc = r.monte_carlo_var(
        lambda dx: -1000.0 * (value(50.0 * (1.0 + dx[0])) - v0),
        [[0.016 * 0.016]],
        0.99,
        100_000,
        7,
    )
    print("Short 1000 calls, 1-day 99% VaR")
    print(
        f"  delta-normal {r.normal_var(1000 * g.delta * 0.8, 0.99):.1f}"
        f"  Cornish-Fisher delta-gamma "
        f"{r.cornish_fisher_var(moments, 0.99):.1f}  Monte Carlo {mc.var:.1f}"
    )

    pca = r.principal_components(_read_csv("rate_changes.csv"))
    total = sum(pca.variances)
    print("PCA of daily changes in 4 rates")
    for k, var in enumerate(pca.variances):
        loads = "".join(f" {x:+.3f}" for x in pca.loadings[k])
        print(
            f"  factor {k + 1}: sd {math.sqrt(var):.4f}, "
            f"{100 * var / total:5.1f}% of variance,{loads}"
        )
    sd = r.pca_std_dev(
        [10.0, 4.0, -5.0, 2.0],
        [pca.loadings[0], pca.loadings[1]],
        [math.sqrt(pca.variances[0]), math.sqrt(pca.variances[1])],
    )
    print(
        f"  exposures (10, 4, -5, 2) per unit move: two-factor sd {sd:.4f}, "
        f"1-day 99% VaR {r.normal_var(sd, 0.99):.4f}"
    )


if __name__ == "__main__":
    main()
