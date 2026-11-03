"""Parity of the Ch 29-34 bindings with the pure-Python package."""

import math

from hypothesis import given
from hypothesis import settings
from hypothesis import strategies as st
import numpy as np
import pytest

import ofod
import ofod_cpp

_CURVE = ([0.25, 1.0, 2.0, 5.0, 10.0], [0.03, 0.035, 0.04, 0.045, 0.05])
_HULL = (
    [0.5, 1.0, 1.5, 2.0, 2.5, 3.0],
    [0.0343, 0.03824, 0.04183, 0.04512, 0.04812, 0.05086],
)


def _curve(lib, data=_CURVE):
    return lib.curves.ZeroCurve(*data)


def _type(lib, call: bool):
    t = lib.instruments.OptionType
    return t.CALL if call else t.PUT


@given(
    st.booleans(),
    st.floats(0.005, 0.1),
    st.floats(0.005, 0.1),
    st.floats(0.05, 0.6),
    st.floats(0.1, 10.0),
)
def test_black_and_bachelier(call, f, k, vol, t) -> None:
    for name in ("black_formula", "bachelier_formula"):
        py = getattr(ofod.pricers, name)(_type(ofod, call), f, k, vol, t)
        cpp = getattr(ofod_cpp.pricers, name)(
            _type(ofod_cpp, call), f, k, vol, t
        )
        assert cpp == pytest.approx(py, rel=1e-12, abs=1e-15)


@given(st.booleans(), st.floats(0.02, 0.07), st.sampled_from([1, 2, 4]))
@settings(max_examples=30)
def test_caps_swaptions(call, strike, freq) -> None:
    def run(lib):
        c = _curve(lib)
        terms = lib.pricers.CapTerms(strike, 4.0, freq)
        cap = lib.pricers.cap_floor_price(_type(lib, call), c, terms, 0.2)
        swap = lib.pricers.forward_swap_rate(c, 2.0, 5.0, freq)
        swaption = lib.pricers.black_swaption_price(
            _type(lib, call), swap.rate, strike, 0.2, 2.0, swap.annuity, 1.0
        )
        return [cap, swap.annuity, swap.rate, swaption]

    assert run(ofod_cpp) == pytest.approx(run(ofod), rel=1e-12)


def test_strip_and_dv01() -> None:
    flat = [0.18, 0.2, 0.21, 0.21, 0.2, 0.19]
    py = ofod.pricers.strip_caplet_volatilities(_curve(ofod), 0.04, 2, flat)
    cpp = ofod_cpp.pricers.strip_caplet_volatilities(
        _curve(ofod_cpp), 0.04, 2, flat
    )
    assert cpp == pytest.approx(py, abs=1e-10)

    def dv01(lib):
        def cap(c):
            terms = lib.pricers.CapTerms(0.04, 5.0)
            return lib.pricers.cap_floor_price(_type(lib, True), c, terms, 0.2)

        return lib.pricers.bucket_dv01(cap, _curve(lib))

    assert dv01(ofod_cpp) == pytest.approx(dv01(ofod), rel=1e-9, abs=1e-15)
    with pytest.raises(ValueError):
        ofod_cpp.pricers.caplet_prices(
            _type(ofod_cpp, True),
            _curve(ofod_cpp),
            ofod_cpp.pricers.CapTerms(0.04, 5.0),
            [0.2],
        )


@given(st.floats(0.01, 0.15), st.floats(0.05, 0.4), st.floats(0.5, 10.0))
def test_adjustments(y, vol, t) -> None:
    for lib_args in (
        ("bond_yield_convexity_adjustment", (y, vol, t, y, 5.0, 2)),
        ("timing_adjustment_factor", (y, vol, vol, 0.7, t, 0.5)),
        ("quanto_adjustment_factor", (-0.3, vol, 0.2, t)),
        ("in_arrears_rate", (y, vol, t, 0.5)),
    ):
        name, args = lib_args
        py = getattr(ofod.pricers, name)(*args)
        cpp = getattr(ofod_cpp.pricers, name)(*args)
        assert cpp == pytest.approx(py, rel=1e-12, abs=1e-16)


@given(
    st.floats(0.05, 1.0), st.floats(0.01, 0.1), st.floats(0.005, 0.05),
    st.floats(0.0, 0.1), st.floats(0.5, 20.0),
)  # fmt: skip
def test_vasicek_cir(a, b, sigma, r, tau) -> None:
    for model in ("Vasicek", "Cir"):
        py = ofod.models.bond_price(
            getattr(ofod.models, model)(a, b, sigma), r, tau
        )
        cpp = ofod_cpp.models.bond_price(
            getattr(ofod_cpp.models, model)(a, b, sigma), r, tau
        )
        assert cpp == pytest.approx(py, rel=1e-12)


def test_hull_white_and_trees() -> None:
    def run(lib):
        c = _curve(lib, _HULL)
        hw = lib.models.HullWhite(c, 0.1, 0.01)
        call = _type(lib, True)
        put = _type(lib, False)
        tree = lib.pricers.ShortRateTree(c, 0.1, 0.01, 0.05, 60)
        bk = lib.pricers.ShortRateTree(
            c, 0.2, 0.25, 0.25, 12, lib.pricers.ShortRateModel.BLACK_KARASINSKI
        )
        flows = [lib.pricers.TreeCashFlow(60, 100.0)]
        return [
            hw.zero_bond_option(put, 1.0, 3.0, 0.92),
            hw.coupon_bond_option(call, 1.0, [2.0, 3.0], [5.0, 105.0], 99.0),
            hw.caplet(call, 1.0, 0.5, 0.04, 1.0),
            *tree.alphas,
            *bk.alphas,
            lib.pricers.tree_swaption(tree, put, 0.05, 20, 60, [20, 40]),
            lib.pricers.tree_bond_option(
                tree, put, lib.instruments.Exercise.AMERICAN, 40, flows, 90.0
            ),
        ]

    assert run(ofod_cpp) == pytest.approx(run(ofod), rel=1e-10, abs=1e-14)
    with pytest.raises(ValueError):
        ofod_cpp.pricers.ShortRateTree(_curve(ofod_cpp), 0.1, 0.01, 0.0, 3)
    with pytest.raises(ValueError):
        ofod_cpp.models.HullWhite(_curve(ofod_cpp), 0.1, 0.0)


def test_calibration() -> None:
    def run(lib):
        c = _curve(lib, _HULL)
        truth = lib.models.HullWhite(c, 0.08, 0.012)
        call = _type(lib, True)
        quotes = [
            lib.models.CapletQuote(
                t, 0.5, 0.05, truth.caplet(call, t, 0.5, 0.05, 1)
            )
            for t in (0.5, 1.0, 1.5, 2.0, 2.5)
        ]
        fit = lib.models.calibrate_hull_white(c, quotes)
        return [fit.a, fit.sigma]

    assert run(ofod_cpp) == pytest.approx(run(ofod), rel=1e-5)


def test_lmm_rebonato_and_payoff_callback() -> None:
    fwd = [0.05 + 0.002 * k for k in range(8)]
    loads = [[0.2 - 0.005 * i, 0.03 * i] for i in range(7)]
    py_spec = ofod.models.LmmSpec(1.0, fwd, loads)
    cpp_spec = ofod_cpp.models.LmmSpec(1.0, fwd, loads)
    assert ofod_cpp.models.rebonato_swaption_volatility(
        cpp_spec, 2, 7
    ) == pytest.approx(
        ofod.models.rebonato_swaption_volatility(py_spec, 2, 7), rel=1e-12
    )
    assert ofod_cpp.models.lmm_volatilities_from_caplets(
        [0.24, 0.22, 0.2]
    ) == pytest.approx(
        ofod.models.lmm_volatilities_from_caplets([0.24, 0.22, 0.2])
    )

    # The same NumPy payoff works per path (C++) and vectorised (Python).
    def caplet(paths):
        return np.maximum(paths.reset(4) - 0.06, 0.0) / paths.numeraire(5)

    py = ofod.models.lmm_monte_carlo(
        py_spec, caplet, ofod.models.LmmOptions(20_000, 1, True)
    )
    cpp = ofod_cpp.models.lmm_monte_carlo(
        cpp_spec, caplet, ofod_cpp.models.LmmOptions(20_000, 1, True)
    )
    se = math.hypot(py.std_error, cpp.std_error)
    assert cpp.price == pytest.approx(py.price, abs=4 * se)


def test_hjm_and_mbs() -> None:
    curve = ([1.0, 5.0], [0.03, 0.05])

    def hjm(lib):
        c = lib.curves.ZeroCurve(*curve)
        fwd = [
            math.log(
                c.discount_factor(i * 0.1) / c.discount_factor(i * 0.1 + 0.1)
            )
            / 0.1
            for i in range(50)
        ]
        spec = lib.models.HjmSpec(
            0.1, fwd, lambda t, m: 0.01 * math.exp(-0.1 * (m - t))
        )
        return lib.models.hjm_zero_bond_option(
            spec, _type(lib, False), 20, 50, 0.85, 20_000, 2
        )

    py, cpp = hjm(ofod), hjm(ofod_cpp)
    assert cpp.price == pytest.approx(
        py.price, abs=4 * math.hypot(py.std_error, cpp.std_error)
    )

    def mbs(lib):
        pool = lib.pricers.MortgagePool(100.0, 0.06, 0.055, 360)
        flows = lib.pricers.pass_through_cash_flows(pool, 165.0)
        v = lib.pricers.value_cash_flows(flows, lib.curves.ZeroCurve.flat(0.05))
        hw = lib.models.HullWhite(lib.curves.ZeroCurve.flat(0.05), 0.1, 0.01)
        mc = lib.pricers.mbs_monte_carlo(
            pool, hw, None, lib.pricers.MbsMcOptions(3000, 4)
        )
        return [v.pass_through, v.interest_only, v.principal_only], mc

    (py_v, py_mc), (cpp_v, cpp_mc) = mbs(ofod), mbs(ofod_cpp)
    assert cpp_v == pytest.approx(py_v, rel=1e-12)
    assert cpp_mc.pass_through == pytest.approx(py_mc.pass_through, rel=5e-3)


def test_exotic_swaps() -> None:
    def run(lib):
        c = _curve(lib)
        p = lib.pricers
        periods = p.swap_schedule(0.0, 5.0, 2, 100.0, 0.045, 10.0, 0.001)
        vols = [0.2] * len(periods)
        leg = p.CmsLeg(5.0, 2, 0.18, 0.7)
        return [
            p.schedule_swap_value(c, periods),
            p.par_fixed_rate(c, periods),
            p.in_arrears_swap_value(c, periods, vols),
            p.cms_swap_value(c, periods, leg, vols),
            p.accrual_fixed_leg_value(c, periods, 0.035, 0.055, 0.2, 10),
            p.cms_rate(p.CmsTerms(0.05, 0.15, 5.0, 10.0, 2, 0.5, 0.045, 0.2)),
        ]

    assert run(ofod_cpp) == pytest.approx(run(ofod), rel=1e-12)
