"""Chapter 20: volatility smiles and surfaces.

Run with `python chapters/ch20_volatility_smiles/py/smiles.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib

_VOLS = [
    [0.142, 0.130, 0.120, 0.131, 0.145],
    [0.140, 0.130, 0.120, 0.131, 0.142],
    [0.141, 0.133, 0.125, 0.134, 0.143],
    [0.147, 0.140, 0.135, 0.140, 0.148],
    [0.150, 0.144, 0.140, 0.145, 0.151],
    [0.148, 0.146, 0.144, 0.147, 0.150],
]
_MATURITIES = [1.0 / 12.0, 0.25, 0.5, 1.0, 2.0, 5.0]
_STRIKES = [0.90, 0.95, 1.00, 1.05, 1.10]


def _skew_vol(strike: float) -> float:
    """A skewed equity smile: sigma(K) = 0.25 - 0.1 (K / S0 - 1)."""
    return 0.25 - 0.1 * (strike / 100.0 - 1.0)


def main() -> None:
    """Reproduces the Chapter 20 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    p, c = lib.pricers, lib.curves
    call_type = lib.instruments.OptionType.CALL

    linear = c.VolSurface(
        _MATURITIES, _STRIKES, _VOLS, c.VolInterpolation.LINEAR_VOL
    )
    variance = c.VolSurface(_MATURITIES, _STRIKES, _VOLS)
    print("Table 20.2 surface, K/S0 = 1.05, 9 months")
    print(
        f"  linear in vol: {100 * linear.volatility(1.05, 0.75):.2f}%   "
        "linear in total variance: "
        f"{100 * variance.volatility(1.05, 0.75):.2f}%"
    )
    term = "".join(
        f" {100 * variance.volatility(1.0, t):.2f}%"
        for t in (0.1, 0.75, 1.5, 3.0, 7.0)
    )
    print("  term structure at K/S0 = 1.00:" + term)

    print("Strikes by delta, S0 = 100, r = 3%, q = 1%, T = 0.5, vol 25%")
    for d in (0.10, 0.25, 0.50, 0.75, 0.90):
        k = p.strike_from_delta(call_type, d, 100.0, 0.03, 0.01, 0.25, 0.5)
        print(f"  {100 * d:>2.0f}-delta call: K = {k:.2f}")

    def call(k: float) -> float:
        return p.bsm_price(call_type, 100.0, k, 0.03, 0.0, _skew_vol(k), 0.5)

    print("Implied density of the skewed smile vs lognormal (T = 0.5)")
    h = 0.5
    for k in range(70, 131, 10):
        g = p.implied_density(call(k - h), call(k), call(k + h), h, 0.03, 0.5)
        ln = p.lognormal_density(100.0, k, 0.03, 0.0, 0.25, 0.5)
        print(f"  K = {k:>3.0f}: implied {g:.5f}  lognormal(25%) {ln:.5f}")

    print("Single large jump: S = 50 moves to 42 or 58 in one month")
    for k in range(44, 57, 2):
        price = p.binary_jump_price(call_type, k, 0.0, 1 / 12, 58.0, 42.0, 0.5)
        iv = p.implied_volatility(call_type, price, 50.0, k, 0.0, 0.0, 1 / 12)
        print(f"  K = {k:.0f}: call {price:.2f}, implied vol {100 * iv:.1f}%")


if __name__ == "__main__":
    main()
