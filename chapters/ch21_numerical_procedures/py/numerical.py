"""Chapter 21: trees, finite differences and Monte Carlo.

Run with `python chapters/ch21_numerical_procedures/py/numerical.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib


def main() -> None:
    """Reproduces the Chapter 21 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers
    ins = lib.instruments
    put, call = ins.OptionType.PUT, ins.OptionType.CALL
    t = 5.0 / 12.0
    market = p.TreeMarket(spot=50.0, rate=0.10, yield_=0.0, volatility=0.40)
    american = ins.VanillaOption(put, ins.Exercise.AMERICAN, 50.0, t)
    european = ins.VanillaOption(put, ins.Exercise.EUROPEAN, 50.0, t)

    print("American put, S0 = K = 50, r = 10%, sigma = 40%, T = 5/12")
    for steps in (5, 30, 50, 100, 500):
        crr = p.price_binomial(
            american, market, steps, p.TreeScheme.COX_ROSS_RUBINSTEIN
        )
        tri = p.price_trinomial(american, market, steps)
        cv = p.control_variate_american_price(american, market, steps)
        print(
            f"  {steps:>3} steps: binomial {crr.price:.4f}  trinomial "
            f"{tri.price:.4f}  control variate {cv:.4f}"
        )
    tree = p.price_binomial(
        european, market, 5, p.TreeScheme.COX_ROSS_RUBINSTEIN
    )
    bs = p.bsm_price(put, 50.0, 50.0, 0.10, 0.0, 0.40, t)
    print(
        f"  European put: 5-step tree {tree.price:.2f}, Black-Scholes {bs:.2f}"
    )
    div_market = p.TreeMarket(spot=52.0, rate=0.10, yield_=0.0, volatility=0.40)
    div = p.price_binomial_with_dividends(
        american, div_market, 5, [ins.CashFlow(3.5 / 12.0, 2.06)]
    )
    print(
        f"  S0 = 52, dividend 2.06 in 3.5 months: 5-step tree {div.price:.2f}"
    )

    hull = p.FdGrid(
        s_min=0.0, s_max=100.0, price_steps=20, time_steps=10, log_space=False
    )
    fine = p.FdGrid(
        s_min=1.0, s_max=500.0, price_steps=400, time_steps=400, log_space=True
    )
    fd, scheme = p.price_finite_difference, p.FdScheme
    rows = [
        (
            "implicit, 20x10, Hull's projection",
            fd(
                american,
                market,
                hull,
                scheme.IMPLICIT,
                p.AmericanSolver.PROJECTION,
            ),
        ),
        ("implicit, 20x10, PSOR", fd(american, market, hull, scheme.IMPLICIT)),
        ("explicit, 20x10", fd(american, market, hull, scheme.EXPLICIT)),
        (
            "Crank-Nicolson, 20x10",
            fd(american, market, hull, scheme.CRANK_NICOLSON),
        ),
        (
            "Crank-Nicolson, log grid 400x400",
            fd(american, market, fine, scheme.CRANK_NICOLSON),
        ),
    ]
    print("Finite differences (Tables 21.5, 21.6)")
    for name, value in rows:
        print(f"  {name:<36} {value:.4f}")

    option = ins.VanillaOption(call, ins.Exercise.EUROPEAN, 40.0, 0.5)
    mc_market = p.TreeMarket(spot=42.0, rate=0.10, yield_=0.0, volatility=0.20)
    bs = p.bsm_price(call, 42.0, 40.0, 0.10, 0.0, 0.20, 0.5)
    print(
        "Monte Carlo, European call S0 = 42, K = 40, 10000 paths "
        f"(BSM {bs:.4f})"
    )
    vr = p.VarianceReduction
    methods = [
        ("plain", vr.NONE),
        ("antithetic", vr.ANTITHETIC),
        ("control variate", vr.CONTROL_VARIATE),
        ("importance sampling", vr.IMPORTANCE_SAMPLING),
        ("stratified", vr.STRATIFIED),
        ("moment matching", vr.MOMENT_MATCHING),
        ("quasi-random (Sobol)", vr.QUASI_RANDOM),
    ]
    for name, method in methods:
        r = p.monte_carlo_european(
            option, mc_market, p.McOptions(paths=10_000, seed=7, method=method)
        )
        print(f"  {name:<22} {r.price:.4f}  s.e. {r.std_error:.4f}")


if __name__ == "__main__":
    main()
