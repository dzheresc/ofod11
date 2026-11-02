"""Chapter 15: lognormal property, volatility, BSM, implied vol, dividends.

Run with `python chapters/ch15_black_scholes/py/black_scholes.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math

_CLOSES = [
    20.00, 20.10, 19.90, 20.00, 20.50, 20.25, 20.90, 20.90, 20.90, 20.75,
    20.75, 21.00, 21.10, 20.90, 20.90, 21.25, 21.40, 21.40, 21.25, 21.75,
    22.00,
]  # fmt: skip


def main() -> None:
    """Reproduces the Chapter 15 worked examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p = lib.pricers
    call = lib.instruments.OptionType.CALL
    put = lib.instruments.OptionType.PUT

    gbm = lib.models.GeometricBrownianMotion(0.16, 0.20)
    ci = gbm.confidence_interval(40.0, 0.5, 1.96)
    print(
        f"S0=40, mu=16%, sigma=20%, T=0.5: 95% range {ci.lower:.2f}-"
        f"{ci.upper:.2f}, E={gbm.expected_value(40.0, 0.5):.2f}, "
        f"sd={math.sqrt(gbm.variance(40.0, 0.5)):.2f}"
    )
    vol = lib.stats.historical_volatility(_CLOSES, 252)
    print(f"Table 15.1 historical volatility: {100 * vol:.1f}%")
    print(
        f"N(0.7693) = {lib.math.norm_cdf(0.7693):.4f} (erfc) vs "
        f"{lib.math.norm_cdf_poly(0.7693):.4f} (Hull's polynomial)"
    )
    c = p.bsm_price(call, 42, 40, 0.10, 0.0, 0.20, 0.5)
    pp = p.bsm_price(put, 42, 40, 0.10, 0.0, 0.20, 0.5)
    print(f"Example 15.6: c = {c:.2f}, p = {pp:.2f}")
    iv = p.implied_volatility(call, 1.875, 21, 20, 0.10, 0.0, 0.25)
    print(f"Implied vol of c = 1.875: {100 * iv:.2f}%")
    divs = [
        lib.instruments.CashFlow(2 / 12, 0.5),
        lib.instruments.CashFlow(5 / 12, 0.5),
    ]
    euro = p.bsm_price_with_dividends(call, 40, 40, 0.09, 0.30, 0.5, divs)
    amer = p.black_american_call_approximation(40, 40, 0.09, 0.30, 0.5, divs)
    print(
        f"Dividends: European call {euro:.2f}, Black's American "
        f"approximation {amer:.2f}"
    )
    w = p.warrant_price(40, 60, 0.03, 0.30, 5.0, 1_000_000, 200_000)
    print(f"Warrant: {w:.2f}")


if __name__ == "__main__":
    main()
