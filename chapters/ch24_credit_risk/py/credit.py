"""Chapter 24: credit risk.

Run with `python chapters/ch24_credit_risk/py/credit.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math


def main() -> None:
    """Reproduces the Chapter 24 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    c, cv = lib.credit, lib.curves

    h = cv.hazard_from_spread(0.02, 0.4)
    print(f"200 bp spread, 40% recovery: hazard rate {h:.4f}")
    h = cv.average_hazard(0.05, 5.0)
    print(f"5% cumulative default by year 5: average hazard {h:.6f}")
    curve = cv.CreditCurve([2.0, 5.0], [0.01, 0.02])
    print("Hazards 1% to year 2, 2% to year 5")
    print("  year  survival  default in year")
    for t in range(1, 6):
        q = curve.survival_probability(t)
        d = curve.default_probability(t - 1.0, t)
        print(f"  {t:4d}  {q:8.6f}  {d:15.6f}")

    m = c.merton_solve(3.0, 0.8, 10.0, 0.05, 1.0)
    riskless = 10.0 * math.exp(-0.05)
    spread = -math.log(m.debt_value / 10.0) - 0.05
    print("Merton: E0 = 3, sigma_E = 80%, D = 10, r = 5%, T = 1")
    print(
        f"  V0 {m.asset_value:.4f}  sigma_V {m.asset_volatility:.4f}  "
        f"P(default) {m.default_probability:.4f}  debt {m.debt_value:.4f} "
        f"vs riskless {riskless:.4f}"
    )
    print(
        f"  expected loss {100 * (1 - m.debt_value / riskless):.2f}%  "
        f"credit spread {1e4 * spread:.1f} bp"
    )

    print("Gaussian copula, Q1 = 1%, Q2 = 2%")
    for rho in (0.0, 0.3, 0.6, 0.9):
        p = c.joint_default_probability(0.01, 0.02, rho)
        print(f"  rho {rho:.1f}: P(both default) {p:.6f}")
    print("Conditional default probability, Q = 2%, rho = 0.1")
    for f in (-3.0, -2.0, 0.0, 2.0):
        p = c.conditional_default_probability(0.02, 0.1, f)
        print(f"  F = {f:+.0f}: {p:.6f}")

    wcdr = c.wcdr(0.02, 0.1, 0.999)
    loss = c.vasicek_credit_loss(100.0, 0.02, 0.4, 0.1, 0.999)
    expected = 100.0 * 0.02 * 0.4
    print(
        f"Vasicek: WCDR(99.9%) {wcdr:.4f}, loss {loss:.2f}M, expected "
        f"{expected:.2f}M, credit VaR {loss - expected:.2f}M"
    )


if __name__ == "__main__":
    main()
