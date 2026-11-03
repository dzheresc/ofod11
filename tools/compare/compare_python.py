#!/usr/bin/env python3
"""Times ofod against other open-source libraries on the same inputs.

Each case prices one instrument in every library that covers it, with the
accuracy settings (steps, grid, paths, quadrature) matched as far as the
libraries allow, and reports the time per call next to the error against a
reference value. See docs/comparison_plan.md for the choice of libraries
and docs/COMPARISON.md for the results.

Install the comparison libraries (they are not dependencies of ofod):

    pip install -r tools/compare/requirements.txt

Then, from the repository root with ofod and ofod_cpp installed:

    OMP_NUM_THREADS=1 NUMBA_NUM_THREADS=1 \
        python tools/compare/compare_python.py \
        --out docs/perf/compare-python.json

Every callable is called once before timing, so Numba's just-in-time
compilation is reported as a separate first-call time and excluded from
the timing. Timings are the best of --repeat runs of timeit's autorange
(at least 0.2 s each), as in tools/profile/bench_python.py.
"""

import argparse
from collections.abc import Callable
import dataclasses
from importlib import metadata
import json
import math
import os
import platform
import sys
import time
import timeit
import types
from typing import Any
import warnings

import arch
from financepy.models import black_scholes_analytic as fp_bs
from financepy.models import black_scholes_mc as fp_mc
from financepy.models import equity_crr_tree as fp_crr
from financepy.models import equity_lsmc as fp_lsmc
from financepy.models import finite_difference as fp_fd
from financepy.models import finite_difference_psor as fp_psor
from financepy.models import gauss_copula_onefactor as fp_copula
from financepy.models import heston as fp_heston
from financepy.utils import global_types as fp_types
from financepy.utils import polyfit as fp_polyfit
import numpy as np
import QuantLib as ql  # noqa: N813
from vollib import black_scholes as vollib_bs
from vollib.black_scholes import implied_volatility as vollib_iv

import ofod
import ofod_cpp

OFOD_PY = "ofod (Python)"
OFOD_CPP = "ofod_cpp"
QUANTLIB = "QuantLib"
FINANCEPY = "FinancePy"
VOLLIB = "vollib"
ARCH = "arch"

_FP_CALL = fp_types.OptionTypes.EUROPEAN_CALL.value
_FP_AMERICAN_PUT = fp_types.OptionTypes.AMERICAN_PUT.value


@dataclasses.dataclass
class Case:
    """One instrument priced by several libraries.

    Attributes:
      name: Short identifier, used by --filter and in the JSON.
      title: One line describing the instrument and the settings.
      reference: The value errors are measured against.
      reference_note: Where the reference comes from.
      runners: Library name to a zero-argument callable returning a value.
      notes: Library name to a note on how its settings differ.
    """

    name: str
    title: str
    reference: float
    reference_note: str
    runners: dict[str, Callable[[], float]]
    notes: dict[str, str] = dataclasses.field(default_factory=dict)


# QuantLib works with dates. A 30/360 day count makes whole months exact
# fractions of a year, so T = 5/12, 0.5, 1 and 10 match the other libraries
# exactly.
_QL_TODAY = ql.Date(15, ql.January, 2026)
_QL_DAY_COUNT = ql.Thirty360(ql.Thirty360.USA)
ql.Settings.instance().evaluationDate = _QL_TODAY


def _ql_date(months: int) -> ql.Date:
    return _QL_TODAY + ql.Period(months, ql.Months)


def _ql_process(
    spot: float, rate: float, vol: float, dividend: float = 0.0
) -> ql.BlackScholesMertonProcess:
    def flat(r: float) -> ql.YieldTermStructureHandle:
        return ql.YieldTermStructureHandle(
            ql.FlatForward(_QL_TODAY, r, _QL_DAY_COUNT, ql.Continuous)
        )

    return ql.BlackScholesMertonProcess(
        ql.QuoteHandle(ql.SimpleQuote(spot)),
        flat(dividend),
        flat(rate),
        ql.BlackVolTermStructureHandle(
            ql.BlackConstantVol(
                _QL_TODAY, ql.NullCalendar(), vol, _QL_DAY_COUNT
            )
        ),
    )


def _ql_npv(option: ql.VanillaOption, engine: Any) -> Callable[[], float]:
    """Prices on every call: setting the engine resets the cached NPV."""

    def run() -> float:
        option.setPricingEngine(engine)
        return option.NPV()

    return run


def _american_put(lib: types.ModuleType, strike: float, maturity: float) -> Any:
    ins = lib.instruments
    return ins.VanillaOption(
        ins.OptionType.PUT, ins.Exercise.AMERICAN, strike, maturity
    )


def _crr_reference(
    spot: float, strike: float, rate: float, vol: float, maturity: float
) -> float:
    """American put by ofod_cpp's CRR tree, mean of 20,000 and 20,001 steps."""
    p = ofod_cpp.pricers
    put = _american_put(ofod_cpp, strike, maturity)
    market = p.TreeMarket(spot, rate, 0.0, vol)
    return 0.5 * (
        p.price_binomial(put, market, 20_000).price
        + p.price_binomial(put, market, 20_001).price
    )


def bsm_case() -> Case:
    """European call, S = 42, K = 40, r = 10%, sigma = 20%, T = 0.5."""
    s, k, r, q, v, t = 42.0, 40.0, 0.1, 0.0, 0.2, 0.5
    df = math.exp(-r * t)
    fwd = s * math.exp((r - q) * t)
    std = v * math.sqrt(t)
    runners = {}
    for lib in (ofod, ofod_cpp):
        p, call = lib.pricers, lib.instruments.OptionType.CALL
        runners[OFOD_PY if lib is ofod else OFOD_CPP] = lambda p=p, call=call: (
            p.bsm_price(call, s, k, r, q, v, t)
        )
    runners[QUANTLIB] = lambda: ql.blackFormula(ql.Option.Call, k, fwd, std, df)
    runners[FINANCEPY] = lambda: float(
        fp_bs.european_value(s, t, k, r, q, v, _FP_CALL)
    )
    runners[VOLLIB] = lambda: vollib_bs.black_scholes("c", s, k, t, r, v)
    # Reference: the formula with math.erfc, which is accurate to rounding.
    d1 = (math.log(s / k) + (r - q + 0.5 * v * v) * t) / std

    def n(x: float) -> float:
        return 0.5 * math.erfc(-x / math.sqrt(2.0))

    ref = s * math.exp(-q * t) * n(d1) - k * df * n(d1 - std)
    return Case(
        "bsm_price",
        "BSM call, S=42 K=40 r=10% sigma=20% T=0.5",
        ref,
        "closed form with erfc",
        runners,
        {
            QUANTLIB: "blackFormula (function, no instrument objects)",
            FINANCEPY: "european_value (Numba ufunc)",
        },
    )


def implied_vol_case() -> Case:
    """Implied vol of a call at 1.875, S = 21, K = 20, r = 10%, T = 0.25."""
    price, s, k, r, q, t = 1.875, 21.0, 20.0, 0.1, 0.0, 0.25
    df = math.exp(-r * t)
    fwd = s * math.exp(r * t)
    runners = {}
    for lib in (ofod, ofod_cpp):
        p, call = lib.pricers, lib.instruments.OptionType.CALL
        runners[OFOD_PY if lib is ofod else OFOD_CPP] = lambda p=p, call=call: (
            p.implied_volatility(call, price, s, k, r, q, t)
        )
    runners[QUANTLIB] = lambda: (
        ql.blackFormulaImpliedStdDev(ql.Option.Call, k, fwd, price, df)
        / math.sqrt(t)
    )
    runners[FINANCEPY] = lambda: float(
        fp_bs.implied_volatility(s, t, k, r, q, price, _FP_CALL)
    )
    runners[VOLLIB] = lambda: vollib_iv.implied_volatility(
        price, s, k, t, r, "c"
    )
    # The reference is the root to machine precision: vollib's Let's Be
    # Rational, polished by one Newton step on the erfc formula.
    ref = vollib_iv.implied_volatility(price, s, k, t, r, "c")
    return Case(
        "implied_volatility",
        "Implied vol, call 1.875, S=21 K=20 r=10% T=0.25",
        ref,
        "Let's Be Rational (machine precision)",
        runners,
        {
            QUANTLIB: "blackFormulaImpliedStdDev, default accuracy 1e-6",
            FINANCEPY: "Newton with bisection fallback (Numba)",
        },
    )


def crr_case(steps: int) -> Case:
    """American put on a CRR tree, S = K = 50, r = 10%, vol 40%, T = 5/12."""
    s, k, r, v, t = 50.0, 50.0, 0.1, 0.4, 5.0 / 12.0
    runners = {}
    for lib in (ofod, ofod_cpp):
        put = _american_put(lib, k, t)
        market = lib.pricers.TreeMarket(s, r, 0.0, v)
        runners[OFOD_PY if lib is ofod else OFOD_CPP] = (
            lambda lib=lib, put=put, market=market: (
                lib.pricers.price_binomial(put, market, steps).price
            )
        )
    option = ql.VanillaOption(
        ql.PlainVanillaPayoff(ql.Option.Put, k),
        ql.AmericanExercise(_QL_TODAY, _ql_date(5)),
    )
    runners[QUANTLIB] = _ql_npv(
        option, ql.BinomialCRRVanillaEngine(_ql_process(s, r, v), steps)
    )
    # FinancePy takes steps per year; is_even = 1 forces an even count, so
    # steps / t per year gives exactly `steps` steps for even `steps`.
    runners[FINANCEPY] = lambda: float(
        fp_crr.crr_tree_val(
            s, r, 0.0, v, round(steps / t), t, _FP_AMERICAN_PUT, k, 1
        )[0]
    )
    return Case(
        f"crr_american_{steps}",
        f"American put, CRR {steps} steps, S=K=50 r=10% sigma=40% T=5/12",
        _crr_reference(s, k, r, v, t),
        "CRR, mean of 20,000 and 20,001 steps",
        runners,
    )


def fd_case() -> Case:
    """American put by Crank-Nicolson, 200 price steps x 100 time steps."""
    s, k, r, v, t = 50.0, 50.0, 0.1, 0.4, 5.0 / 12.0
    runners = {}
    for lib in (ofod, ofod_cpp):
        p = lib.pricers
        put = _american_put(lib, k, t)
        market = p.TreeMarket(s, r, 0.0, v)
        grid = p.FdGrid(s_max=200.0, price_steps=200, time_steps=100)
        for solver, suffix in (
            ("BRENNAN_SCHWARTZ", ""),
            ("PROJECTION", " projection"),
            ("PSOR", " PSOR"),
        ):
            label = (OFOD_PY if lib is ofod else OFOD_CPP) + suffix
            runners[label] = (
                lambda p=p, put=put, market=market, grid=grid, solver=solver: (
                    p.price_finite_difference(
                        put,
                        market,
                        grid,
                        p.FdScheme.CRANK_NICOLSON,
                        getattr(p.AmericanSolver, solver),
                    )
                )
            )
    option = ql.VanillaOption(
        ql.PlainVanillaPayoff(ql.Option.Put, k),
        ql.AmericanExercise(_QL_TODAY, _ql_date(5)),
    )
    runners[QUANTLIB] = _ql_npv(
        option,
        ql.FdBlackScholesVanillaEngine(
            _ql_process(s, r, v), 100, 200, 0, ql.FdmSchemeDesc.CrankNicolson()
        ),
    )
    runners[FINANCEPY + " PSOR"] = lambda: float(
        fp_psor.black_scholes_fd_psor(
            s,
            v,
            t,
            k,
            r,
            0.0,
            _FP_AMERICAN_PUT,
            num_time_steps=100,
            num_samples=200,
            theta=0.5,
        )
    )
    # black_scholes_fd takes steps per year and truncates steps_per_year * t.
    runners[FINANCEPY] = lambda: float(
        fp_fd.black_scholes_fd(
            s,
            v,
            t,
            k,
            r,
            0.0,
            _FP_AMERICAN_PUT,
            num_steps_per_year=100 / t + 1e-9,
            num_samples=200,
            theta=0.5,
        )
    )
    return Case(
        "fd_american_cn_200x100",
        "American put, Crank-Nicolson, 200 price x 100 time steps",
        _crr_reference(s, k, r, v, t),
        "CRR, mean of 20,000 and 20,001 steps",
        runners,
        {
            QUANTLIB: "log-spot grid, 200 points; early exercise by "
            "projection after each step",
            FINANCEPY: "log-spot grid of +-5 sd, 200 points; projection",
            FINANCEPY + " PSOR": "log-spot grid of +-5 sd; PSOR to 1e-13",
            OFOD_PY: "spot grid 0-200; Brennan-Schwartz",
            OFOD_CPP: "spot grid 0-200; Brennan-Schwartz",
            OFOD_CPP + " projection": "spot grid 0-200; projection, as "
            "QuantLib and FinancePy",
        },
    )


def mc_case() -> Case:
    """European call by Monte Carlo, 100,000 terminal draws."""
    s, k, r, v, t, paths = 50.0, 40.0, 0.1, 0.4, 0.5, 100_000
    runners = {}
    for lib in (ofod, ofod_cpp):
        p = lib.pricers
        call = lib.instruments.VanillaOption(strike=k, maturity=t)
        market = p.TreeMarket(s, r, 0.0, v)
        opts = p.McOptions(paths=paths)
        runners[OFOD_PY if lib is ofod else OFOD_CPP] = (
            lambda p=p, call=call, market=market, opts=opts: (
                p.monte_carlo_european(call, market, opts).price
            )
        )
    zig_call = ofod_cpp.instruments.VanillaOption(strike=k, maturity=t)
    zig_market = ofod_cpp.pricers.TreeMarket(s, r, 0.0, v)
    zig_opts = ofod_cpp.pricers.McOptions(
        paths=paths, rng=ofod_cpp.pricers.Rng.ZIGGURAT
    )
    runners[OFOD_CPP + " ziggurat"] = lambda: (
        ofod_cpp.pricers.monte_carlo_european(
            zig_call, zig_market, zig_opts
        ).price
    )
    option = ql.VanillaOption(
        ql.PlainVanillaPayoff(ql.Option.Call, k),
        ql.EuropeanExercise(_ql_date(6)),
    )
    runners[QUANTLIB] = _ql_npv(
        option,
        ql.MCEuropeanEngine(
            _ql_process(s, r, v),
            "pseudorandom",
            timeSteps=1,
            requiredSamples=paths,
            seed=42,
        ),
    )
    runners[FINANCEPY] = lambda: float(
        fp_mc.value_mc_numba_noanti(s, t, k, r, 0.0, v, _FP_CALL, paths, 42, 0)
    )
    std_error = ofod_cpp.pricers.monte_carlo_european(
        ofod_cpp.instruments.VanillaOption(strike=k, maturity=t),
        ofod_cpp.pricers.TreeMarket(s, r, 0.0, v),
        ofod_cpp.pricers.McOptions(paths=paths),
    ).std_error
    return Case(
        "mc_european_100k",
        "European call, Monte Carlo, 100,000 draws, "
        "S=50 K=40 r=10% sigma=40% T=0.5",
        ofod_cpp.pricers.bsm_price(
            ofod_cpp.instruments.OptionType.CALL, s, k, r, 0.0, v, t
        ),
        f"BSM closed form; one standard error is {std_error:.3f}",
        runners,
        {
            OFOD_CPP: "mt19937_64 + std::normal_distribution (default)",
            OFOD_CPP + " ziggurat": "xoshiro256++ + ziggurat (Rng.ZIGGURAT)",
            QUANTLIB: "Mersenne Twister + inverse normal, one time step",
            FINANCEPY: "value_mc_numba_noanti (Numba loop)",
        },
    )


def heston_case() -> Case:
    """Heston call, S = K = 100, r = 3%, T = 1, v0 = theta = 4%, rho = -0.7."""
    s, k, r, t = 100.0, 100.0, 0.03, 1.0
    v0, kappa, theta, sigma, rho = 0.04, 2.0, 0.04, 0.3, -0.7
    runners = {}
    for lib in (ofod, ofod_cpp):
        p = lib.pricers
        params = p.HestonParams(
            v0=v0, kappa=kappa, theta=theta, sigma=sigma, rho=rho
        )
        market = p.TreeMarket(s, r, 0.0, 0.0)
        call = lib.instruments.OptionType.CALL
        runners[OFOD_PY if lib is ofod else OFOD_CPP] = (
            lambda p=p, call=call, market=market, params=params: p.heston_price(
                call, market, k, t, params
            )
        )
    process = ql.HestonProcess(
        ql.YieldTermStructureHandle(
            ql.FlatForward(_QL_TODAY, r, _QL_DAY_COUNT, ql.Continuous)
        ),
        ql.YieldTermStructureHandle(
            ql.FlatForward(_QL_TODAY, 0.0, _QL_DAY_COUNT, ql.Continuous)
        ),
        ql.QuoteHandle(ql.SimpleQuote(s)),
        v0,
        kappa,
        theta,
        sigma,
        rho,
    )
    model = ql.HestonModel(process)
    option = ql.VanillaOption(
        ql.PlainVanillaPayoff(ql.Option.Call, k),
        ql.EuropeanExercise(_ql_date(12)),
    )
    runners[QUANTLIB] = _ql_npv(option, ql.AnalyticHestonEngine(model))
    runners[QUANTLIB + " COS"] = _ql_npv(option, ql.COSHestonEngine(model))
    # FinancePy's Heston methods read the maturity as
    # (expiry_dt - value_dt) / 365 and the strike from the option, so plain
    # numbers stand in for the dates to give T = 1 exactly.
    fp_model = fp_heston.Heston(v0, kappa, theta, sigma, rho)
    fp_option = types.SimpleNamespace(expiry_dt=365.0, strike_price=k)
    runners[FINANCEPY] = lambda: float(
        fp_model.value_lewis(0.0, fp_option, s, r, 0.0)
    )
    option.setPricingEngine(ql.AnalyticHestonEngine(model, 1e-13, 1_000_000))
    ref = option.NPV()
    return Case(
        "heston_price_1y",
        "Heston call, S=K=100 r=3% T=1, v0=theta=0.04 "
        "kappa=2 sigma=0.3 rho=-0.7",
        ref,
        "QuantLib adaptive Gauss-Lobatto at 1e-13",
        runners,
        {
            QUANTLIB: "AnalyticHestonEngine default (Gauss-Laguerre, "
            "144 points)",
            QUANTLIB + " COS": "COSHestonEngine defaults",
            FINANCEPY: "value_lewis (SciPy quad over the Lewis integral)",
        },
    )


def lsm_case() -> Case:
    """Longstaff-Schwartz put, S = 36, K = 40, r = 6%, sigma = 20%, T = 1."""
    s, k, r, v, t, paths, dates = 36.0, 40.0, 0.06, 0.2, 1.0, 10_000, 50
    runners = {}
    for lib in (ofod, ofod_cpp):
        p = lib.pricers
        market = p.TreeMarket(s, r, 0.0, v)
        put = lib.instruments.OptionType.PUT
        opts = p.LsmOptions(paths=paths)
        runners[OFOD_PY if lib is ofod else OFOD_CPP] = (
            lambda p=p, put=put, market=market, opts=opts: (
                p.longstaff_schwartz(put, market, k, t, opts).price
            )
        )
    option = ql.VanillaOption(
        ql.PlainVanillaPayoff(ql.Option.Put, k),
        ql.AmericanExercise(_QL_TODAY, _ql_date(12)),
    )
    runners[QUANTLIB] = _ql_npv(
        option,
        ql.MCAmericanEngine(
            _ql_process(s, r, v),
            "pseudorandom",
            timeSteps=dates,
            requiredSamples=paths,
            seed=42,
            polynomOrder=2,
            polynomType=ql.LsmBasisSystem.Monomial,
            nCalibrationSamples=paths,
        ),
    )
    # FinancePy 1.1.2 falls back to NumPy for its polynomial fit when the
    # Numba version fails to compile, and the fallback names a function that
    # does not exist (_eval_poly_numpy). Point the name at the real one.
    if not hasattr(fp_polyfit, "_eval_poly_numpy"):
        fp_polyfit._eval_poly_numpy = fp_polyfit._eval_polynomial_numpy
    fit = fp_lsmc.BoundaryFitTypes.POLYNOMIAL.value
    runners[FINANCEPY] = lambda: float(
        fp_lsmc.equity_lsmc(
            s, r, 0.0, v, paths, dates, t, _FP_AMERICAN_PUT, k, 2, fit, 0, 42
        )
    )
    std_error = ofod_cpp.pricers.longstaff_schwartz(
        ofod_cpp.instruments.OptionType.PUT,
        ofod_cpp.pricers.TreeMarket(s, r, 0.0, v),
        k,
        t,
        ofod_cpp.pricers.LsmOptions(paths=paths),
    ).std_error
    return Case(
        "longstaff_schwartz_10k",
        "American put, Longstaff-Schwartz, "
        "10,000 paths x 50 dates, S=36 K=40 r=6% sigma=20% T=1",
        _crr_reference(s, k, r, v, t),
        "CRR, mean of 20,000 and 20,001 steps (American, so the "
        "50-date Bermudan sits a little below); one standard error is "
        f"{std_error:.3f}",
        runners,
        {
            QUANTLIB: "separate 10,000 calibration paths, then 10,000 "
            "pricing paths",
            FINANCEPY: "antithetic pairs (5,000 x 2); polynomial degree 2",
        },
    )


def hull_white_case() -> Case:
    """Receiver Bermudan swaption in Hull-White, 500 tree steps, 10 years."""
    rate, a, sigma, strike, steps = 0.04, 0.1, 0.01, 0.04, 500
    exercise = list(range(steps // 10, steps, steps // 10))
    runners = {}
    for lib in (ofod, ofod_cpp):
        p = lib.pricers
        curve = lib.curves.ZeroCurve.flat(rate)
        put = lib.instruments.OptionType.PUT

        def run(p=p, curve=curve, put=put) -> float:
            tree = p.ShortRateTree(curve, a, sigma, 10.0 / steps, steps)
            return p.tree_swaption(
                tree, put, strike, steps // 10, steps, exercise
            )

        runners[OFOD_PY if lib is ofod else OFOD_CPP] = run

    # QuantLib: a 10-year annual swap on a 1-year index forecast from the
    # same flat curve (so the floating leg is worth 1 - P(0, 10) at par), with
    # exercise on each of years 1 to 9.
    curve = ql.YieldTermStructureHandle(
        ql.FlatForward(_QL_TODAY, rate, _QL_DAY_COUNT, ql.Continuous)
    )
    index = ql.IborIndex(
        "Annual",
        ql.Period(1, ql.Years),
        0,
        ql.USDCurrency(),
        ql.NullCalendar(),
        ql.Unadjusted,
        False,
        _QL_DAY_COUNT,
        curve,
    )
    schedule = ql.Schedule(
        _QL_TODAY,
        _ql_date(120),
        ql.Period(1, ql.Years),
        ql.NullCalendar(),
        ql.Unadjusted,
        ql.Unadjusted,
        ql.DateGeneration.Forward,
        False,
    )
    swap = ql.VanillaSwap(
        ql.Swap.Receiver,
        1.0,
        schedule,
        strike,
        _QL_DAY_COUNT,
        schedule,
        index,
        0.0,
        _QL_DAY_COUNT,
    )
    swap.setPricingEngine(ql.DiscountingSwapEngine(curve))
    swaption = ql.Swaption(
        swap, ql.BermudanExercise([_ql_date(12 * y) for y in range(1, 10)])
    )
    model = ql.HullWhite(curve, a, sigma)

    def ql_run() -> float:
        swaption.setPricingEngine(ql.TreeSwaptionEngine(model, steps))
        return swaption.NPV()

    runners[QUANTLIB] = ql_run
    swaption.setPricingEngine(ql.FdHullWhiteSwaptionEngine(model, 2000, 800))
    ref = swaption.NPV()
    return Case(
        "hull_white_bermudan_500",
        "Receiver Bermudan swaption, "
        "Hull-White a=0.1 sigma=1%, 10y annual, exercise years 1-9, "
        "500 tree steps",
        ref,
        "QuantLib FdHullWhiteSwaptionEngine, 2,000 x 800",
        runners,
        {
            QUANTLIB: "TreeSwaptionEngine, 500 steps plus the exercise "
            "and payment dates; tree build included"
        },
    )


def tranche_case() -> Case:
    """Mezzanine 3-6% tranche, 125 names, Gaussian copula with rho = 0.3."""
    names, hazard, recovery, corr = 125, 0.01, 0.4, 0.3
    maturity, frequency, rate, points = 5.0, 4, 0.05, 64
    k1, k2 = 0.03, 0.06
    runners = {}
    for lib in (ofod, ofod_cpp):
        c = lib.credit
        mezz = c.Tranche(attachment=k1, detachment=k2)
        pool, terms = c.CdoPool(), c.CdoTerms()
        curve = lib.curves.ZeroCurve.flat(rate)
        runners[OFOD_PY if lib is ofod else OFOD_CPP] = (
            lambda c=c, mezz=mezz, pool=pool, terms=terms, curve=curve: (
                c.tranche_spread(mezz, pool, corr, terms, curve)
            )
        )

    # FinancePy: its tranche survival probability (full recursion over the
    # 125 names, rectangle rule in the market factor) at each quarterly
    # date, with the premium and protection legs assembled as in ofod.
    recoveries = np.full(names, recovery)
    betas = np.full(names, math.sqrt(corr))

    def fp_run() -> float:
        annuity = protection = previous = start = 0.0
        for i in range(1, int(maturity * frequency) + 1):
            end = i / frequency
            surv = np.full(names, math.exp(-hazard * end))
            loss = 1.0 - fp_copula.tranche_surv_prob_recursion(
                k1, k2, names, surv, recoveries, betas, points
            )
            df_mid = math.exp(-rate * 0.5 * (start + end))
            annuity += (end - start) * (1.0 - loss) * math.exp(
                -rate * end
            ) + 0.5 * (end - start) * (loss - previous) * df_mid
            protection += (loss - previous) * df_mid
            previous, start = loss, end
        return protection / annuity

    runners[FINANCEPY] = fp_run
    fine = ofod_cpp.credit.CdoTerms()
    fine.quadrature_points = 180
    ref = ofod_cpp.credit.tranche_spread(
        ofod_cpp.credit.Tranche(attachment=k1, detachment=k2),
        ofod_cpp.credit.CdoPool(),
        corr,
        fine,
        ofod_cpp.curves.ZeroCurve.flat(rate),
    )
    return Case(
        "tranche_spread_125",
        "Mezzanine 3-6% tranche spread, 125 names, "
        "h=1% R=40% rho=0.3, 5y quarterly",
        ref,
        "ofod_cpp with 180 Gauss-Hermite points (199 and 200 points "
        "return garbage, see docs/COMPARISON.md)",
        runners,
        {
            OFOD_PY: "64 Gauss-Hermite points, binomial loss distribution",
            OFOD_CPP: "64 Gauss-Hermite points, binomial loss distribution",
            FINANCEPY: "tranche_surv_prob_recursion with 64 rectangle "
            "steps on [-6, 6]; legs assembled by this script",
        },
    )


def _hull_garch_log_likelihood(
    returns: np.ndarray, omega: float, alpha: float, beta: float
) -> float:
    """Hull's GARCH(1,1) log likelihood: v_1 = u_0^2, sum -(ln v + u^2 / v)."""
    v = returns[0] ** 2
    total = 0.0
    for u in returns[1:]:
        total -= math.log(v) + u * u / v
        v = omega + alpha * u * u + beta * v
    return total


def garch_case() -> Case:
    """GARCH(1,1) maximum likelihood fit to 1,000 simulated GARCH returns."""
    # Daily returns from a GARCH(1,1) with omega = 2e-6, alpha = 0.1 and
    # beta = 0.88 (long-run volatility 1% a day), seeded so every library
    # fits the same series.
    rng = np.random.default_rng(2026)
    array = np.empty(1000)
    v = 1e-4
    for i in range(array.size):
        array[i] = math.sqrt(v) * rng.standard_normal()
        v = 2e-6 + 0.1 * array[i] ** 2 + 0.88 * v
    returns = array.tolist()
    runners = {}
    fits = {}
    for lib in (ofod, ofod_cpp):
        label = OFOD_PY if lib is ofod else OFOD_CPP

        def run(lib=lib, label=label) -> float:
            g = lib.stats.fit_garch11(returns)
            fits[label] = (g.omega, g.alpha, g.beta)
            return _hull_garch_log_likelihood(array, g.omega, g.alpha, g.beta)

        runners[label] = run

    def arch_run() -> float:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            res = arch.arch_model(
                array, mean="Zero", vol="GARCH", p=1, q=1, rescale=False
            ).fit(disp="off")
        omega, alpha, beta = (
            float(res.params[n]) for n in ("omega", "alpha[1]", "beta[1]")
        )
        fits[ARCH] = (omega, alpha, beta)
        return _hull_garch_log_likelihood(array, omega, alpha, beta)

    runners[ARCH] = arch_run
    case = Case(
        "fit_garch11_1000",
        "GARCH(1,1) MLE, 1,000 simulated returns (omega=2e-6 alpha=0.1 "
        "beta=0.88); value is "
        "Hull's log likelihood at the fitted parameters",
        math.nan,
        "the best log likelihood found (higher is better)",
        runners,
        {
            ARCH: "arch_model(mean='Zero', vol='GARCH'), SLSQP, "
            "backcast start variance",
            OFOD_CPP: "Nelder-Mead on Hull's likelihood",
        },
    )
    case.fits = fits  # type: ignore[attr-defined]
    return case


CASES: dict[str, Callable[[], Case]] = {
    "bsm_price": bsm_case,
    "implied_volatility": implied_vol_case,
    "crr_american_100": lambda: crr_case(100),
    "crr_american_1000": lambda: crr_case(1000),
    "fd_american_cn_200x100": fd_case,
    "mc_european_100k": mc_case,
    "heston_price_1y": heston_case,
    "longstaff_schwartz_10k": lsm_case,
    "hull_white_bermudan_500": hull_white_case,
    "tranche_spread_125": tranche_case,
    "fit_garch11_1000": garch_case,
}


def _time(fn: Callable[[], Any], repeat: int) -> float:
    """Best time per call of `repeat` runs, each lasting at least 0.2 s."""
    timer = timeit.Timer(fn)
    number, _ = timer.autorange()
    number = max(number, 1)
    runs = timer.repeat(repeat=repeat, number=number)
    return min(runs) / number


def _format(seconds: float) -> str:
    for unit, scale in (("s", 1.0), ("ms", 1e-3), ("us", 1e-6)):
        if seconds >= scale:
            return f"{seconds / scale:.3g} {unit}"
    return f"{seconds / 1e-9:.3g} ns"


def _run_case(case: Case, repeat: int) -> dict[str, Any]:
    """Times every runner of a case and prints its Markdown table."""
    print(f"\n### {case.name}: {case.title}\n")
    print(f"Reference {case.reference:.10g} ({case.reference_note}).\n")
    print("| Library | Time | First call | Value | Error |")
    print("| --- | --- | --- | --- | --- |")
    rows = []
    for library, fn in case.runners.items():
        start = time.perf_counter()
        value = float(fn())
        first = time.perf_counter() - start
        seconds = _time(fn, repeat)
        error = (
            value - case.reference if math.isfinite(case.reference) else None
        )
        rows.append(
            {
                "library": library,
                "seconds": seconds,
                "first_call_seconds": first,
                "value": value,
                "error": error,
                "note": case.notes.get(library, ""),
            }
        )
        error_text = f"{error:.2e}" if error is not None else "-"
        print(
            f"| {library} | {_format(seconds)} | {_format(first)} | "
            f"{value:.10g} | {error_text} |",
            flush=True,
        )
    result = dataclasses.asdict(case)
    del result["runners"]
    result["rows"] = rows
    if hasattr(case, "fits"):
        result["fits"] = case.fits
        for library, (omega, alpha, beta) in case.fits.items():
            print(
                f"\n{library}: omega={omega:.4g} alpha={alpha:.4f} "
                f"beta={beta:.4f}",
                end="",
            )
        print()
    return result


def main() -> int:
    """Runs the selected cases and prints one Markdown table per case."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", help="write results as JSON to this path")
    parser.add_argument("--filter", default="", help="substring of case names")
    parser.add_argument("--repeat", type=int, default=5)
    args = parser.parse_args()

    results = [
        _run_case(make(), args.repeat)
        for name, make in CASES.items()
        if args.filter in name
    ]
    if args.out:
        versions = {"python": platform.python_version()}
        for package in (
            "numpy",
            "numba",
            "QuantLib",
            "financepy",
            "arch",
            "vollib",
        ):
            versions[package] = metadata.version(package)
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(
                {
                    "machine": platform.machine(),
                    "cpus": os.cpu_count(),
                    "versions": versions,
                    "cases": results,
                },
                f,
                indent=2,
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
