"""Chapter 23: estimating volatilities and correlations.

Run with `python chapters/ch23_volatility_correlation/py/volatility.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math
import pathlib

_DATA = pathlib.Path(__file__).resolve().parents[3] / "data" / "market"


def main() -> None:
    """Reproduces the Chapter 23 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    s = lib.stats
    v = s.ewma_variance_update(1e-4, 0.02, 0.90)
    print(f"EWMA, lambda 0.90: vol 1%, return 2% -> {100 * math.sqrt(v):.2f}%")
    hull = s.Garch11(0.000002, 0.13, 0.86)
    vl = hull.long_run_variance()
    print(
        f"GARCH(0.000002, 0.13, 0.86): V_L = {vl:.4f} "
        f"({100 * math.sqrt(vl):.2f}% a day); vol 1.6%, return 1% -> "
        f"{100 * math.sqrt(hull.update(0.016 * 0.016, 0.01)):.2f}%"
    )
    cov = s.ewma_covariance_update(0.6 * 0.01 * 0.02, 0.005, 0.025, 0.95)
    vx = s.ewma_variance_update(1e-4, 0.005, 0.95)
    vy = s.ewma_variance_update(4e-4, 0.025, 0.95)
    print(
        f"EWMA correlation, lambda 0.95: 0.6 -> {cov / math.sqrt(vx * vy):.4f}"
        f" (vols {100 * math.sqrt(vx):.3f}%, {100 * math.sqrt(vy):.3f}%)"
    )

    u = [
        float(line)
        for line in (_DATA / "garch_returns.csv").read_text().splitlines()
        if line and not line.startswith("#")
    ]
    fit = s.fit_garch11(u)

    def ljung_box_of(variances: list[float]) -> float:
        z = [u[i] * u[i] / variances[i] for i in range(1, len(u))]
        return s.ljung_box(z, 15)

    print(
        f"GARCH(1,1) fit to {len(u)} simulated returns "
        "(truth 2e-6, 0.13, 0.86):"
    )
    print(
        f"  omega {fit.omega:.3e}, alpha {fit.alpha:.4f}, beta "
        f"{fit.beta:.4f}, long-run vol "
        f"{100 * math.sqrt(fit.long_run_variance()):.3f}% a day"
    )
    print(
        f"  log likelihood: GARCH {fit.log_likelihood(u):.2f}, EWMA(0.94) "
        f"{s.ewma_log_likelihood(u, 0.94):.2f}"
    )
    variances = list(fit.variances(u))
    print(
        f"  Ljung-Box (15 lags, critical 25.0): u^2 "
        f"{ljung_box_of([1.0] * len(u)):.1f}, u^2 / sigma^2 "
        f"{ljung_box_of(variances):.1f}"
    )
    today = fit.update(variances[-1], u[-1])
    print(
        f"Volatility term structure from today's {100 * math.sqrt(today):.2f}%"
        " a day (annualised)"
    )
    for days in (10.0, 30.0, 50.0, 100.0, 500.0):
        vol = fit.term_structure_volatility(today, days)
        print(f"  {days:>3.0f} days: {100 * vol:.2f}%")

    m = [[1.0, 0.0, 0.9], [0.0, 1.0, 0.9], [0.9, 0.9, 1.0]]
    smallest = lib.math.symmetric_eigen(m).values[-1]
    psd = "yes" if s.is_positive_semidefinite(m) else "no"
    print(
        f"[[1,0,0.9],[0,1,0.9],[0.9,0.9,1]] positive semi-definite: {psd} "
        f"(smallest eigenvalue {smallest:.4f})"
    )


if __name__ == "__main__":
    main()
