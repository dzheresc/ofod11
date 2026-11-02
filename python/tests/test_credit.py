"""Credit curves, Merton, copulas, CDS, CDO tranches and XVA (Ch 9, 24, 25)."""

import math

import numpy as np
import pytest
from scipy import integrate
from scipy import stats

from ofod import credit
from ofod import curves
from ofod import math as om


@pytest.mark.parametrize("rho", [-0.95, -0.5, 0.0, 0.3, 0.8, 0.97])
@pytest.mark.parametrize("a,b", [(-1.0, 0.5), (0.7, 1.3), (-2.0, -1.5)])
def test_bivariate_normal_matches_scipy(rho: float, a: float, b: float) -> None:
    mvn = stats.multivariate_normal([0.0, 0.0], [[1.0, rho], [rho, 1.0]])
    assert om.bivariate_norm_cdf(a, b, rho) == pytest.approx(
        mvn.cdf([a, b]), abs=1e-7
    )


def test_bivariate_normal_limits() -> None:
    assert om.bivariate_norm_cdf(0.0, 0.0, 0.5) == pytest.approx(1.0 / 3.0)
    assert om.bivariate_norm_cdf(0.4, -0.2, 1.0) == pytest.approx(
        om.norm_cdf(-0.2)
    )
    assert om.bivariate_norm_cdf(0.4, -0.2, -1.0) == pytest.approx(
        max(om.norm_cdf(0.4) - om.norm_cdf(0.2), 0.0), abs=1e-14
    )


def test_gauss_hermite_integrates_normal_moments() -> None:
    rule = om.gauss_hermite_normal(20)
    for k, expected in [(0, 1.0), (2, 1.0), (4, 3.0), (6, 15.0)]:
        total = sum(
            w * x**k for x, w in zip(rule.nodes, rule.weights, strict=True)
        )
        assert total == pytest.approx(expected, rel=1e-12)
    with pytest.raises(ValueError):
        om.gauss_hermite(0)


def test_credit_curve_survival_and_validation() -> None:
    c = curves.CreditCurve([2.0, 5.0], [0.01, 0.02])
    assert c.survival_probability(4.0) == pytest.approx(math.exp(-0.06))
    assert c.survival_probability(7.0) == pytest.approx(math.exp(-0.12))
    assert c.default_probability(1.0, 3.0) == pytest.approx(
        math.exp(-0.01) - math.exp(-0.04)
    )
    assert c.hazard(3.0) == 0.02
    with pytest.raises(ValueError):
        curves.CreditCurve([2.0, 1.0], [0.01, 0.02])
    with pytest.raises(ValueError):
        curves.CreditCurve([1.0], [-0.01])


def test_merton_reproduces_hull_example() -> None:
    m = credit.merton_solve(3.0, 0.8, 10.0, 0.05, 1.0)
    assert m.asset_value == pytest.approx(12.40, abs=5e-3)
    assert m.asset_volatility == pytest.approx(0.2123, abs=5e-4)
    assert m.default_probability == pytest.approx(0.127, abs=1e-3)
    with pytest.raises(ValueError):
        credit.merton_solve(-1.0, 0.8, 10.0, 0.05, 1.0)


def test_one_factor_formulas() -> None:
    assert credit.wcdr(0.02, 0.1, 0.999) == pytest.approx(0.128, abs=5e-4)
    assert credit.wcdr(0.02, 0.0, 0.999) == pytest.approx(0.02)
    # Conditional PDs average back to the unconditional PD.
    rule = om.gauss_hermite_normal(64)
    avg = sum(
        w * credit.conditional_default_probability(0.03, 0.25, f)
        for f, w in zip(rule.nodes, rule.weights, strict=True)
    )
    assert avg == pytest.approx(0.03, rel=1e-8)
    indep = credit.joint_default_probability(0.01, 0.02, 0.0)
    assert indep == pytest.approx(2e-4, rel=1e-8)


def test_default_count_distribution_is_binomial() -> None:
    dist = credit.default_count_distribution([0.1] * 8)
    assert dist == pytest.approx(stats.binom.pmf(np.arange(9), 8, 0.1).tolist())
    assert sum(credit.default_count_distribution([0.1, 0.5, 0.9])) == (
        pytest.approx(1.0)
    )


def test_cds_par_spread_near_credit_triangle() -> None:
    terms = credit.CdsTerms(5.0, 4, 0.4)
    c = curves.CreditCurve.flat(0.02)
    z = curves.ZeroCurve.flat(0.05)
    assert credit.cds_par_spread(terms, c, z) == pytest.approx(
        0.02 * 0.6, rel=1e-2
    )
    s = credit.cds_par_spread(terms, c, z)
    assert credit.cds_buyer_value(terms, s, c, z) == pytest.approx(
        0.0, abs=1e-15
    )


def test_bootstrap_reprices_quotes() -> None:
    mats = [1.0, 2.0, 3.0, 5.0, 7.0]
    spreads = [0.005, 0.0065, 0.008, 0.01, 0.011]
    z = curves.ZeroCurve.flat(0.05)
    curve = credit.bootstrap_credit_curve(mats, spreads, 4, 0.4, z)
    for m, s in zip(mats, spreads, strict=True):
        terms = credit.CdsTerms(m, 4, 0.4)
        assert credit.cds_par_spread(terms, curve, z) == pytest.approx(
            s, abs=1e-12
        )
    with pytest.raises(ValueError):
        credit.bootstrap_credit_curve([2.0, 1.0], [0.01, 0.01], 4, 0.4, z)


def test_cds_option_put_call_parity() -> None:
    call = credit.cds_option_price(True, 0.012, 0.01, 0.5, 1.0, 3.5)
    put = credit.cds_option_price(False, 0.012, 0.01, 0.5, 1.0, 3.5)
    assert call - put == pytest.approx(3.5 * 0.002)


def test_equity_tranche_spread_falls_with_correlation() -> None:
    pool = credit.CdoPool()
    terms = credit.CdoTerms()
    z = curves.ZeroCurve.flat(0.035)
    equity = credit.Tranche(0.0, 0.03)
    low = credit.tranche_spread(equity, pool, 0.05, terms, z)
    high = credit.tranche_spread(equity, pool, 0.5, terms, z)
    assert high < low
    # The 0-100% tranche is the index: its loss does not depend on rho.
    full = credit.Tranche(0.0, 1.0)
    a = credit.expected_tranche_loss(full, pool, 0.0, 5.0)
    b = credit.expected_tranche_loss(full, pool, 0.6, 5.0)
    assert a == pytest.approx(b, rel=1e-6)
    assert a == pytest.approx(0.6 * (1.0 - math.exp(-0.05)), rel=1e-6)


def test_implied_correlation_round_trip() -> None:
    pool, terms = credit.CdoPool(), credit.CdoTerms()
    z = curves.ZeroCurve.flat(0.035)
    t = credit.Tranche(0.0, 0.03)
    s = credit.tranche_spread(t, pool, 0.2, terms, z)
    rho = credit.implied_tranche_correlation(t, pool, s, terms, z)
    assert rho == pytest.approx(0.2, abs=1e-8)
    with pytest.raises(ValueError):
        credit.implied_tranche_correlation(t, pool, 10.0, terms, z)


def test_first_to_default_bounds() -> None:
    terms = credit.BasketTerms()
    r = credit.nth_to_default_spread(terms, 0.05, 20_000, 1)
    single = 0.01 * 0.6
    assert single < r.spread < 10 * single
    second = credit.nth_to_default_spread(
        credit.BasketTerms(nth=2), 0.05, 20_000, 1
    )
    assert second.spread < r.spread
    with pytest.raises(ValueError):
        credit.nth_to_default_spread(credit.BasketTerms(nth=11), 0.05, 100, 1)


def test_profile_and_threshold() -> None:
    values = [[1.0, -2.0], [3.0, 4.0], [-1.0, 10.0]]
    p = credit.profile_from_paths([0.5, 1.0], values, 0.5, threshold=5.0)
    assert p.expected_exposure == pytest.approx([4.0 / 3.0, 9.0 / 3.0])
    assert p.expected_negative_exposure == pytest.approx([1.0 / 3.0, 2.0 / 3.0])
    assert p.potential_future_exposure == pytest.approx([1.0, 4.0])


def test_forward_exposure_matches_black_scholes() -> None:
    p = credit.simulate_forward_exposure(
        1600.0, 1500.0, 0.05, 0.2, 1.0, 4, 100_000, 3
    )
    ee = p.expected_exposure[-1]
    assert ee == pytest.approx(226.29, abs=3.0)


def test_cva_fva_and_dva() -> None:
    times = [1.0, 2.0]
    curve = curves.CreditCurve.flat(0.02)
    v = credit.cva(times, [10.0, 8.0], curve, 0.4)
    q1 = 1.0 - math.exp(-0.02)
    q2 = math.exp(-0.02) - math.exp(-0.04)
    assert v == pytest.approx(0.6 * (q1 * 10.0 + q2 * 8.0))
    assert credit.dva(times, [10.0, 8.0], curve, 0.4) == pytest.approx(v)
    assert credit.funding_value_adjustment(
        times, [10.0, 8.0], 0.01
    ) == pytest.approx(0.18)
    with pytest.raises(ValueError):
        credit.cva([1.0], [1.0, 2.0], curve, 0.4)


def test_expected_loss_integrates_with_quad() -> None:
    # Independent check of the 3-6% tranche loss with adaptive quadrature.
    pool = credit.CdoPool()
    t = credit.Tranche(0.03, 0.06)
    q = 1.0 - math.exp(-0.05)
    rho = 0.3

    def integrand(f: float) -> float:
        p = credit.conditional_default_probability(q, rho, f)
        k = np.arange(126)
        pmf = stats.binom.pmf(k, 125, p)
        loss = np.clip((0.6 * k / 125 - 0.03) / 0.03, 0.0, 1.0)
        return float(pmf @ loss) * stats.norm.pdf(f)

    ref, _ = integrate.quad(integrand, -10.0, 10.0, limit=200)
    got = credit.expected_tranche_loss(t, pool, rho, 5.0, 64)
    assert got == pytest.approx(ref, rel=1e-4)
