"""Interest rate derivatives (Hull, Ch 29-34)."""

import math

import numpy as np
import pytest

from ofod import models
from ofod import pricers
from ofod.curves import ZeroCurve
from ofod.instruments.option import Exercise
from ofod.instruments.option import OptionType

CALL = OptionType.CALL
PUT = OptionType.PUT


def _upward() -> ZeroCurve:
    return ZeroCurve(
        [0.25, 1.0, 2.0, 5.0, 10.0], [0.03, 0.035, 0.04, 0.045, 0.05]
    )


def _hull_curve() -> ZeroCurve:
    return ZeroCurve(
        [0.5, 1.0, 1.5, 2.0, 2.5, 3.0],
        [0.0343, 0.03824, 0.04183, 0.04512, 0.04812, 0.05086],
    )


def test_hull_examples_29() -> None:
    coupons = 50 * math.exp(-0.25 * 0.09) + 50 * math.exp(-0.75 * 0.095)
    df = math.exp(-0.1 * 10 / 12)
    fwd = pricers.forward_bond_price(960, coupons, df)
    assert fwd == pytest.approx(939.68, abs=0.005)
    price = pricers.black_bond_option_price(CALL, fwd, 1000, df, 0.09, 10 / 12)
    assert price == pytest.approx(9.49, abs=0.005)
    caplet = pricers.black_caplet_price(
        CALL, 0.07, 0.08, 0.2, 1.0, 0.25, math.exp(-0.065 * 1.25), 10_000
    )
    assert caplet == pytest.approx(5.19, abs=0.005)
    swap = pricers.forward_swap_rate(ZeroCurve.flat(0.06), 5, 3, 2)
    assert swap.rate == pytest.approx(0.06091, abs=1e-5)
    swaption = pricers.black_swaption_price(
        CALL, swap.rate, 0.062, 0.2, 5, swap.annuity, 100
    )
    assert swaption == pytest.approx(2.07, abs=0.005)


def test_caps_and_stripping() -> None:
    curve = _upward()
    terms = pricers.CapTerms(0.04, 5.0, 4)
    assert pricers.caplet_count(terms) == 19
    cap = pricers.cap_floor_price(CALL, curve, terms, 0.2)
    floor = pricers.cap_floor_price(PUT, curve, terms, 0.2)
    swap = sum(
        0.25
        * (pricers.simple_forward_rate(curve, 0.25 * k, 0.25 * k + 0.25) - 0.04)
        * curve.discount_factor(0.25 * k + 0.25)
        for k in range(1, 20)
    )
    assert cap - floor == pytest.approx(swap, abs=1e-12)
    humped = [0.18, 0.2, 0.21, 0.21, 0.2, 0.19]
    spot = pricers.strip_caplet_volatilities(curve, 0.04, 2, humped)
    for i, flat in enumerate(humped):
        t = pricers.CapTerms(0.04, 0.5 * (i + 2), 2)
        total = sum(pricers.caplet_prices(CALL, curve, t, spot[: i + 1]))
        assert total == pytest.approx(
            pricers.cap_floor_price(CALL, curve, t, flat), abs=1e-10
        )
    with pytest.raises(ValueError):
        pricers.caplet_prices(CALL, curve, terms, [0.2])


def test_bachelier_and_dv01() -> None:
    diff = pricers.bachelier_formula(
        CALL, -0.002, 0.001, 0.006, 2
    ) - pricers.bachelier_formula(PUT, -0.002, 0.001, 0.006, 2)
    assert diff == pytest.approx(-0.003, abs=1e-15)
    curve = _upward()

    def cap(c: ZeroCurve) -> float:
        return pricers.cap_floor_price(
            CALL, c, pricers.CapTerms(0.04, 5.0), 0.2
        )

    buckets = pricers.bucket_dv01(cap, curve)
    assert buckets[-1] == 0.0
    assert sum(buckets) == pytest.approx(
        pricers.parallel_dv01(cap, curve), rel=1e-2
    )


def test_adjustments() -> None:
    adj = pricers.bond_yield_convexity_adjustment(0.1, 0.2, 3, 0, 1, 1)
    assert 1000 * (0.1 + adj) / 1.1**3 == pytest.approx(75.95, abs=0.005)
    rate = (0.1 + adj) * pricers.timing_adjustment_factor(
        0.1, 0.2, 0.2, 1, 3, 1
    )
    assert 1000 * rate / 1.1**4 == pytest.approx(68.30, abs=0.005)
    assert pricers.quanto_adjustment_factor(-0.3, 0.12, 0.25, 2) == (
        pytest.approx(math.exp(-0.018))
    )
    assert pricers.in_arrears_rate(0.05, 0.0, 3, 0.5) == 0.05


def test_exotic_swaps() -> None:
    curve = _upward()
    periods = pricers.swap_schedule(1.0, 6.0, 2, 100.0, 0.0, 8.0)
    par = pricers.par_fixed_rate(curve, periods)
    periods = [
        pricers.SwapPeriod(p.start, p.end, p.notional, par) for p in periods
    ]
    assert pricers.schedule_swap_value(curve, periods) == pytest.approx(
        0.0, abs=1e-12
    )
    plain = pricers.swap_schedule(0.0, 3.0, 4, 1.0, 0.05)
    leg = pricers.CmsLeg(5.0, 2, 0.0)
    cms = sum(
        0.25
        * (0.05 - pricers.forward_swap_rate(curve, p.start, 5.0, 2).rate)
        * curve.discount_factor(p.end)
        for p in plain
    )
    zero = [0.0] * 12
    assert pricers.cms_swap_value(curve, plain, leg, zero) == pytest.approx(
        cms, abs=1e-14
    )
    fixed = sum(0.25 * 0.05 * curve.discount_factor(p.end) for p in plain)
    assert pricers.accrual_fixed_leg_value(
        curve, plain, 0.0, 1e9, 0.2, 30
    ) == pytest.approx(fixed, abs=1e-12)


def test_vasicek_cir() -> None:
    v = models.Vasicek(0.1, 0.1, 0.02)
    call = models.zero_bond_option_price(v, CALL, 0.1, 1, 5, 0.7)
    put = models.zero_bond_option_price(v, PUT, 0.1, 1, 5, 0.7)
    assert call - put == pytest.approx(
        models.bond_price(v, 0.1, 5) - 0.7 * models.bond_price(v, 0.1, 1),
        abs=1e-14,
    )
    assert models.zero_rate(v, 0.05, 2000) == pytest.approx(0.08, abs=2e-4)
    c = models.Cir(0.2, 0.05, 0.1)
    rng = np.random.default_rng(1)
    means = [
        models.simulate_cir(c, 0.04, 2.0, 1, int(s))[1]
        for s in rng.integers(0, 2**31, 4000)
    ]
    assert np.mean(means) == pytest.approx(
        0.05 - 0.01 * math.exp(-0.4), abs=6e-4
    )
    truth = models.Vasicek(0.5, 0.04, 0.01)
    dt = 1 / 252
    phi = math.exp(-truth.a * dt)
    sd = truth.sigma * math.sqrt((1 - phi**2) / (2 * truth.a))
    shocks = np.random.default_rng(9).standard_normal(252 * 200)
    r = [0.03]
    for e in shocks:
        r.append(truth.b + (r[-1] - truth.b) * phi + sd * e)
    fit = models.estimate_vasicek(r, dt)
    assert fit.sigma == pytest.approx(0.01, rel=1e-2)
    assert fit.b == pytest.approx(0.04, abs=3e-3)
    with pytest.raises(ValueError):
        models.estimate_vasicek([0.01, 0.02], dt)


def test_hull_white_and_tree() -> None:
    curve = _hull_curve()
    hw = models.HullWhite(curve, 0.1, 0.01)
    assert hw.bond_price(0.0, 2.5, hw.instantaneous_forward(0.0)) == (
        pytest.approx(curve.discount_factor(2.5), abs=1e-9)
    )
    tree = pricers.ShortRateTree(curve, 0.1, 0.01, 1.0, 3)
    assert tree.jmax == 2
    assert tree.alphas == pytest.approx([0.03824, 0.05205, 0.06252], abs=1e-5)
    top = tree.branch_at(2)
    assert (top.up, top.mid, top.down) == pytest.approx(
        (0.8867, 0.0267, 0.0867), abs=1e-4
    )
    for i in range(4):
        assert sum(tree.arrow_debreu(i)) == pytest.approx(
            curve.discount_factor(i), abs=1e-12
        )
    fine = pricers.ShortRateTree(curve, 0.1, 0.01, 0.02, 150)
    once = [50]
    euro = pricers.tree_swaption(fine, PUT, 0.05, 50, 150, once)
    analytic = hw.coupon_bond_option(CALL, 1.0, [2.0, 3.0], [0.05, 1.05], 1.0)
    assert euro == pytest.approx(analytic, abs=2e-5)
    assert pricers.tree_swaption(fine, PUT, 0.05, 50, 150, [50, 100]) > euro
    zero = [pricers.TreeCashFlow(150, 100.0)]
    e = pricers.tree_bond_option(fine, PUT, Exercise.EUROPEAN, 100, zero, 90)
    a = pricers.tree_bond_option(fine, PUT, Exercise.AMERICAN, 100, zero, 90)
    assert a >= e
    bk = pricers.ShortRateTree(
        curve, 0.22, 0.25, 0.25, 12, pricers.ShortRateModel.BLACK_KARASINSKI
    )
    assert bk.zero_bond_values(0, 12)[0] == pytest.approx(
        curve.discount_factor(3.0), abs=1e-12
    )
    with pytest.raises(ValueError):
        pricers.ShortRateTree(curve, 0.1, 0.01, 0.0, 3)


def test_calibration() -> None:
    curve = _hull_curve()
    truth = models.HullWhite(curve, 0.08, 0.012)
    quotes = [
        models.CapletQuote(t, 0.5, k, truth.caplet(CALL, t, 0.5, k, 1.0))
        for t in (0.5, 1.0, 1.5, 2.0, 2.5)
        for k in (0.04, 0.055)
    ]
    fit = models.calibrate_hull_white(curve, quotes)
    assert fit.a == pytest.approx(0.08, abs=1e-4)
    assert fit.sigma == pytest.approx(0.012, abs=1e-6)


def test_lmm() -> None:
    lam = models.lmm_volatilities_from_caplets([0.24, 0.22, 0.20])
    assert lam == pytest.approx([0.24, 0.1980, 0.1523], abs=5e-5)
    fwd = [0.05 + 0.002 * k for k in range(10)]
    loadings = [
        [0.15 + 0.06 * math.exp(-0.5 * i) - 0.03 * i / 9] for i in range(9)
    ]
    spec = models.LmmSpec(1.0, fwd, loadings)
    p = models.lmm_discount_factors(spec)

    def bond(paths: models.LmmPaths) -> np.ndarray:
        return 1.0 / paths.numeraire(paths.size)

    mc = models.lmm_monte_carlo(
        spec, bond, models.LmmOptions(20_000, 5, predictor_corrector=True)
    )
    assert mc.price == pytest.approx(p[-1], abs=3 * mc.std_error + 2e-4)
    k = 6
    vol = math.sqrt(sum(x[0] ** 2 for x in loadings[:k]) / k)
    black = pricers.black_caplet_price(
        CALL, fwd[k], 0.06, vol, k, 1.0, p[k + 1], 1
    )

    def caplet(paths: models.LmmPaths) -> np.ndarray:
        return np.maximum(paths.reset(k) - 0.06, 0.0) / paths.numeraire(k + 1)

    mc = models.lmm_monte_carlo(
        spec, caplet, models.LmmOptions(40_000, 7, predictor_corrector=True)
    )
    assert mc.price == pytest.approx(black, abs=4 * mc.std_error)
    assert models.rebonato_swaption_volatility(spec, 3, 8) > 0.1
    with pytest.raises(ValueError):
        models.rebonato_swaption_volatility(spec, 0, 4)


def test_hjm_matches_hull_white() -> None:
    curve = ZeroCurve([1.0, 5.0], [0.03, 0.05])
    dt = 0.05
    fwd = [
        math.log(
            curve.discount_factor(i * dt) / curve.discount_factor((i + 1) * dt)
        )
        / dt
        for i in range(100)
    ]
    spec = models.HjmSpec(dt, fwd, lambda t, m: 0.01 * math.exp(-0.1 * (m - t)))
    bond = models.hjm_zero_bond_option(spec, CALL, 40, 100, 0.0, 20_000, 3)
    assert bond.price == pytest.approx(
        curve.discount_factor(5.0), abs=3 * bond.std_error
    )
    hw = models.HullWhite(curve, 0.1, 0.01)
    put = models.hjm_zero_bond_option(spec, PUT, 40, 100, 0.82, 20_000, 4)
    assert put.price == pytest.approx(
        hw.zero_bond_option(PUT, 2.0, 5.0, 0.82), abs=4 * put.std_error + 1e-4
    )


def test_mbs() -> None:
    pool = pricers.MortgagePool(100.0, 0.06, 0.055, 360)
    level = pricers.pass_through_cash_flows(pool, 0.0)
    assert sum(f.scheduled + f.prepaid for f in level) == pytest.approx(100)
    flat = ZeroCurve.flat(0.05)
    slow = pricers.value_cash_flows(
        pricers.pass_through_cash_flows(pool, 100), flat
    )
    fast = pricers.value_cash_flows(
        pricers.pass_through_cash_flows(pool, 300), flat
    )
    assert fast.interest_only < slow.interest_only
    assert fast.principal_only > slow.principal_only
    hw = models.HullWhite(flat, 0.1, 0.01)
    mc = pricers.mbs_monte_carlo(
        pool,
        hw,
        pricers.RefinancingModel(0.06, 0.0, max_cpr=1.0),
        pricers.MbsMcOptions(4000, 2),
    )
    assert mc.pass_through == pytest.approx(slow.pass_through, rel=2e-3)
