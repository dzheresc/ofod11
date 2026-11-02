"""Exotic options and advanced models (Hull, Ch 26, 27)."""

import math

import numpy as np
import pytest
from scipy import special
from scipy import stats

from ofod import math as om
from ofod import pricers as p
from ofod.instruments.option import OptionType

CALL, PUT = OptionType.CALL, OptionType.PUT
M = p.TreeMarket


def _bsm(
    t: OptionType, s: float, k: float, r: float, q: float, v: float, tt: float
):
    return p.bsm_price(t, s, k, r, q, v, tt)


def test_hull_examples() -> None:
    assert p.gap_option_price(
        PUT, M(500_000, 0.05, 0.0, 0.2), 400_000, 350_000, 1.0
    ) == pytest.approx(1895.68, abs=0.01)
    look = M(50.0, 0.1, 0.0, 0.4)
    assert p.floating_lookback_price(CALL, look, 50.0, 0.25) == pytest.approx(
        8.04, abs=5e-3
    )
    assert p.arithmetic_asian_price(CALL, look, 50.0, 1.0) == pytest.approx(
        5.62, abs=5e-3
    )
    terms = p.ConvertibleTerms(100.0, 2.0, 0.75, 113.0, 0.4, 0.01)
    assert p.convertible_bond_price(
        M(50.0, 0.05, 0.0, 0.3), terms, 3
    ) == pytest.approx(106.93, abs=5e-3)


@pytest.mark.parametrize("k", [80.0, 100.0, 130.0])
def test_binaries_add_to_vanilla(k: float) -> None:
    m = M(100.0, 0.05, 0.02, 0.3)
    for t, sign in ((CALL, 1.0), (PUT, -1.0)):
        combo = sign * (
            p.asset_or_nothing_price(t, m, k, 0.7)
            - p.cash_or_nothing_price(t, m, k, 0.7, k)
        )
        assert combo == pytest.approx(_bsm(t, 100, k, 0.05, 0.02, 0.3, 0.7))


def test_compound_parity() -> None:
    m = M(50.0, 0.05, 0.01, 0.3)
    pay = 3.0 * math.exp(-0.05 * 0.25)
    price = {
        c: p.compound_option_price(c, m, 3.0, 0.25, 50.0, 1.0)
        for c in p.CompoundType
    }
    ct = p.CompoundType
    assert price[ct.CALL_ON_CALL] - price[ct.PUT_ON_CALL] == pytest.approx(
        _bsm(CALL, 50, 50, 0.05, 0.01, 0.3, 1.0) - pay, abs=1e-9
    )
    assert price[ct.CALL_ON_PUT] - price[ct.PUT_ON_PUT] == pytest.approx(
        _bsm(PUT, 50, 50, 0.05, 0.01, 0.3, 1.0) - pay, abs=1e-9
    )
    with pytest.raises(ValueError):
        p.compound_option_price(ct.CALL_ON_PUT, m, 60.0, 0.25, 50.0, 1.0)


@pytest.mark.parametrize("level", [90.0, 95.0, 105.0, 110.0])
@pytest.mark.parametrize("otype", [CALL, PUT])
def test_barrier_in_out_parity(level: float, otype: OptionType) -> None:
    m = M(100.0, 0.08, 0.04, 0.25)
    bt = p.BarrierType
    down = level < 100.0
    out = bt.DOWN_AND_OUT if down else bt.UP_AND_OUT
    knock_in = bt.DOWN_AND_IN if down else bt.UP_AND_IN
    total = p.barrier_option_price(
        out, otype, m, 100.0, level, 0.5
    ) + p.barrier_option_price(knock_in, otype, m, 100.0, level, 0.5)
    assert total == pytest.approx(_bsm(otype, 100, 100, 0.08, 0.04, 0.25, 0.5))


def test_barrier_matches_monte_carlo() -> None:
    m = M(100.0, 0.08, 0.04, 0.25)
    steps, paths, level = 100, 100_000, 95.0
    dt = 0.5 / steps
    rng = np.random.default_rng(3)
    z = rng.standard_normal((paths, steps))
    log_s = np.cumsum((0.04 - 0.5 * 0.25**2) * dt + 0.25 * math.sqrt(dt) * z, 1)
    s = 100.0 * np.exp(log_s)
    alive = s.min(axis=1) > level
    v = math.exp(-0.04) * np.where(
        alive, np.maximum(s[:, -1] - 100.0, 0.0), 0.0
    )
    shift = math.exp(0.5826 * 0.25 * math.sqrt(dt))
    exact = p.barrier_option_price(
        p.BarrierType.DOWN_AND_OUT, CALL, m, 100.0, level / shift, 0.5
    )
    assert v.mean() == pytest.approx(exact, abs=4 * v.std() / math.sqrt(paths))


def test_lookbacks_asians_and_exchange() -> None:
    m = M(50.0, 0.1, 0.02, 0.4)
    assert p.fixed_lookback_price(PUT, m, 55.0, 45.0, 1e-10) == pytest.approx(
        10.0, abs=1e-6
    )
    # Discrete geometric average converges on the continuous formula.
    n, paths = 200, 100_000
    rng = np.random.default_rng(1)
    dt = 1.0 / n
    z = rng.standard_normal((paths, n))
    log_s = math.log(50.0) + np.cumsum(
        (0.1 - 0.02 - 0.08) * dt + 0.4 * math.sqrt(dt) * z, axis=1
    )
    g = np.exp(log_s.mean(axis=1))
    v = math.exp(-0.1) * np.maximum(g - 50.0, 0.0)
    assert v.mean() == pytest.approx(
        p.geometric_asian_price(CALL, m, 50.0, 1.0),
        abs=4 * v.std() / math.sqrt(paths) + 0.02,
    )
    riskless = p.ExchangeAssets(100.0, 95.0, 0.02, 0.05, 0.3, 0.0, 0.0)
    assert p.exchange_option_price(riskless, 1.0) == pytest.approx(
        _bsm(CALL, 100, 95, 0.05, 0.02, 0.3, 1.0)
    )
    with pytest.raises(ValueError):
        p.floating_lookback_price(CALL, M(50, 0.05, 0.05, 0.3), 50.0, 1.0)


def test_chooser_perpetual_and_variance_swap() -> None:
    m = M(50.0, 0.08, 0.0, 0.25)
    assert p.chooser_price(m, 50.0, 0.25, 0.5) == pytest.approx(
        6.1071, abs=5e-5
    )
    assert p.perpetual_american_price(
        PUT, M(10.0, 0.05, 0.0, 0.3), 50.0
    ) == pytest.approx(40.0)
    with pytest.raises(ValueError):
        p.perpetual_american_price(CALL, M(50.0, 0.05, 0.0, 0.3), 50.0)
    strikes = [20.0 + 0.5 * i for i in range(761)]
    var = p.variance_swap_fair_variance(
        M(100.0, 0.04, 0.01, 0.25), 0.5, strikes, lambda k: 0.25
    )
    assert var == pytest.approx(0.0625, abs=2e-5)


def test_special_functions_and_cev() -> None:
    assert om.regularized_gamma_p(30.0, 50.0) == pytest.approx(
        special.gammainc(30.0, 50.0)
    )
    assert om.noncentral_chi_square_cdf(5.0, 3.0, 2.0) == pytest.approx(
        stats.ncx2.cdf(5.0, 3.0, 2.0)
    )
    assert math.isnan(om.regularized_gamma_p(-1.0, 1.0))
    m = M(100.0, 0.05, 0.01, 0.2 * 100**0.5)
    call = p.cev_price(CALL, m, 100.0, 1.0, 0.5)
    put = p.cev_price(PUT, m, 100.0, 1.0, 0.5)
    assert call - put == pytest.approx(
        100 * math.exp(-0.01) - 100 * math.exp(-0.05)
    )
    with pytest.raises(ValueError):
        p.cev_price(CALL, m, 100.0, 1.0, 1.0)


def test_gauss_legendre_and_solve() -> None:
    q = om.gauss_legendre(8, 0.0, 2.0)
    assert sum(w * x**5 for x, w in zip(q.nodes, q.weights, strict=True)) == (
        pytest.approx(2.0**6 / 6.0)
    )
    assert om.solve_linear(
        [[2.0, 1.0], [1.0, 3.0]], [3.0, 5.0]
    ) == pytest.approx([0.8, 1.4])
    with pytest.raises(ValueError):
        om.solve_linear([[1.0, 2.0], [2.0, 4.0]], [1.0, 2.0])


def test_merton_jump_and_heston() -> None:
    m = M(100.0, 0.05, 0.01, 0.2)
    assert p.merton_jump_price(
        CALL, m, 105.0, 1.0, p.JumpParams(0.0, 0.0, 0.1)
    ) == pytest.approx(_bsm(CALL, 100, 105, 0.05, 0.01, 0.2, 1.0))
    h = M(100.0, 0.03, 0.01, 0.0)
    flat = p.HestonParams(0.04, 1.0, 0.04, 1e-4, 0.0)
    for k in (70.0, 100.0, 140.0):
        assert p.heston_price(CALL, h, k, 1.0, flat) == pytest.approx(
            _bsm(CALL, 100, k, 0.03, 0.01, 0.2, 1.0), abs=1e-6
        )
    hp = p.HestonParams(0.04, 2.0, 0.04, 0.5, -0.7)
    exact = p.heston_price(CALL, h, 100.0, 1.0, hp)
    mc = p.heston_monte_carlo(CALL, h, 100.0, 1.0, hp, 100, 50_000, 3)
    assert mc.price == pytest.approx(exact, abs=4 * mc.std_error + 0.02)


def test_longstaff_schwartz() -> None:
    m = M(36.0, 0.06, 0.0, 0.2)
    lsm = p.longstaff_schwartz(PUT, m, 40.0, 1.0, p.LsmOptions(paths=50_000))
    assert lsm.price == pytest.approx(4.478, abs=0.04)
    with pytest.raises(ValueError):
        p.longstaff_schwartz(PUT, m, 40.0, 1.0, p.LsmOptions(basis_degree=0))
