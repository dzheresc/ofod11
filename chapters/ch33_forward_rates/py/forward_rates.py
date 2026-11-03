"""Chapter 33: HJM, the LIBOR market model and agency MBS.

Run with `python chapters/ch33_forward_rates/py/forward_rates.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.

The payoffs use NumPy ufuncs, so they work on every path at once in the
pure-Python library and on one path at a time in the bindings.
"""

import argparse
import importlib
import math
from typing import Any

import numpy as np

_PATHS = 10_000


def _hjm(lib: Any) -> None:
    md = lib.models
    curve = lib.curves.ZeroCurve([1.0, 5.0], [0.03, 0.05])
    dt = 0.05
    fwd = [
        math.log(
            curve.discount_factor(i * dt) / curve.discount_factor((i + 1) * dt)
        )
        / dt
        for i in range(100)
    ]
    spec = md.HjmSpec(dt, fwd, lambda t, m: 0.01 * math.exp(-0.1 * (m - t)))
    ot = lib.instruments.OptionType
    bond = md.hjm_zero_bond_option(spec, ot.CALL, 40, 100, 0.0, _PATHS, 1)
    put = md.hjm_zero_bond_option(spec, ot.PUT, 40, 100, 0.82, _PATHS, 2)
    hw = md.HullWhite(curve, 0.1, 0.01)
    print(
        "HJM with v(t, T) = 0.01 e^(-0.1 (T - t)), dt = 0.05: 5-year zero "
        f"{bond.price:.5f} (curve {curve.discount_factor(5.0):.5f}); 2-year "
        f"put on it at 0.82 {put.price:.5f} (se {put.std_error:.5f}), "
        f"Hull-White {hw.zero_bond_option(ot.PUT, 2.0, 5.0, 0.82):.5f}"
    )


def _spec(lib: Any, lambdas: list[float], factors: int) -> Any:
    if factors == 1:
        loads = [[x] for x in lambdas]
    else:
        loads = [
            [x * math.cos(0.2 * i), x * math.sin(0.2 * i)]
            for i, x in enumerate(lambdas)
        ]
    return lib.models.LmmSpec(1.0, [0.05] * 10, loads)


def _plain(path: Any) -> Any:
    value = 0.0
    for k in range(1, path.size):
        value = value + np.maximum(path.reset(k) - 0.05, 0.0) / path.numeraire(
            k + 1
        )
    return 100.0 * value


def _ratchet(path: Any) -> Any:
    value = 0.0
    for k in range(1, path.size):
        strike = path.reset(k - 1) + 0.0025
        value = value + np.maximum(
            path.reset(k) - strike, 0.0
        ) / path.numeraire(k + 1)
    return 100.0 * value


def _sticky(path: Any) -> Any:
    value = 0.0
    capped = path.reset(0)
    for k in range(1, path.size):
        strike = capped + 0.0025
        value = value + np.maximum(
            path.reset(k) - strike, 0.0
        ) / path.numeraire(k + 1)
        capped = np.minimum(path.reset(k), strike)
    return 100.0 * value


def _lmm(lib: Any) -> None:
    md = lib.models
    p = lib.pricers
    call = lib.instruments.OptionType.CALL
    hull = md.lmm_volatilities_from_caplets([0.24, 0.22, 0.20])
    print(
        f"Caplet vols 24%, 22%, 20% give Lambda = {100 * hull[0]:.2f}%, "
        f"{100 * hull[1]:.2f}%, {100 * hull[2]:.2f}%"
    )
    caplet_vols = [0.155, 0.1825, 0.1791, 0.1774, 0.1727, 0.1679, 0.163,
                   0.1601, 0.1576]  # fmt: skip
    lambdas = md.lmm_volatilities_from_caplets(caplet_vols)
    print(
        "LMM: 10 annual forwards at 5%, Lambda (%):"
        + "".join(f" {100 * x:.2f}" for x in lambdas)
    )
    df = md.lmm_discount_factors(_spec(lib, lambdas, 1))
    black = 0.0
    for k in range(1, 10):
        black += p.black_caplet_price(
            call, 0.05, 0.05, caplet_vols[k - 1], k, 1.0, df[k + 1], 100.0
        )
    print(
        "  caps per 100 (spot measure, predictor-corrector, "
        f"{_PATHS} paths); Black ATM cap {black:.4f}"
    )
    print("  factors  ATM cap   ratchet  sticky")
    for factors in (1, 2):
        spec = _spec(lib, lambdas, factors)
        options = md.LmmOptions(_PATHS, 11, True)
        prices = [
            md.lmm_monte_carlo(spec, f, options).price
            for f in (_plain, _ratchet, _sticky)
        ]
        print(
            f"  {factors:7}  {prices[0]:7.4f}  {prices[1]:7.4f}  "
            f"{prices[2]:7.4f}"
        )

    two = _spec(lib, lambdas, 2)
    start, end = 3, 8
    vol = md.rebonato_swaption_volatility(two, start, end)
    annuity = 0.0
    for k in range(start, end):
        annuity += df[k + 1]
    s0 = (df[start] - df[end]) / annuity

    def swaption(path: Any) -> Any:
        discount = 1.0
        a = 0.0
        for k in range(start, end):
            discount = discount / (1.0 + path.forward(start, k))
            a = a + discount
        return (
            100.0
            * np.maximum(1.0 - discount - s0 * a, 0.0)
            / path.numeraire(start)
        )

    mc = md.lmm_monte_carlo(two, swaption, md.LmmOptions(_PATHS, 12, True))
    black_swaption = p.black_swaption_price(
        call, s0, s0, vol, start, annuity, 100.0
    )
    print(
        f"3 x 5 ATM payer swaption per 100: Rebonato vol {100 * vol:.2f}% "
        f"gives Black {black_swaption:.4f}; LMM Monte Carlo {mc.price:.4f} "
        f"(se {mc.std_error:.4f})"
    )


def _mbs(lib: Any) -> None:
    p = lib.pricers
    pool = p.MortgagePool(100.0, 0.06, 0.055, 360)
    flat = lib.curves.ZeroCurve.flat(0.05)
    print("MBS: 30-year pool, WAC 6%, pass-through 5.5%, flat 5% curve")
    print("  PSA   pass-through      IO      PO")
    for speed in (0.0, 50.0, 100.0, 200.0, 400.0):
        v = p.value_cash_flows(p.pass_through_cash_flows(pool, speed), flat)
        print(
            f"  {speed:3.0f}   {v.pass_through:12.4f}  {v.interest_only:6.2f}  "
            f"{v.principal_only:6.2f}"
        )
    print(
        "  Monte Carlo with refinancing (Hull-White a = 0.1, sigma = 1%, "
        "2,000 paths)"
    )
    print("  rates   pass-through      IO      PO")
    for shift in (-0.01, 0.0, 0.01):
        hw = lib.models.HullWhite(
            lib.curves.ZeroCurve.flat(0.05 + shift), 0.1, 0.01
        )
        v = p.mbs_monte_carlo(pool, hw, None, p.MbsMcOptions(2_000, 3))
        print(
            f"  {100 * (0.05 + shift):4.0f}%   {v.pass_through:12.4f}  "
            f"{v.interest_only:6.2f}  {v.principal_only:6.2f}"
        )


def main() -> None:
    """Reproduces the Chapter 33 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    _hjm(lib)
    _lmm(lib)
    _mbs(lib)


if __name__ == "__main__":
    main()
