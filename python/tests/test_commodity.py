"""Commodities, weather and insurance (Ch 35) and real options (Ch 36)."""

import math

import numpy as np
import pytest

from ofod.instruments.option import Exercise
from ofod.instruments.option import OptionType
from ofod.instruments.option import VanillaOption
from ofod.models import commodity
from ofod.pricers import binomial_tree
from ofod.pricers import real_options as ro
from ofod.pricers import weather_insurance as wi
from ofod.pricers.commodity_tree import CommodityTree
from ofod.pricers.commodity_tree import commodity_tree_option


def test_seasonal_fit_recovers_curve() -> None:
    raw = [
        0.2 * math.cos(2 * math.pi * m / 12) + 0.03 * (m % 3) for m in range(12)
    ]
    mean = sum(raw) / 12
    truth = commodity.SeasonalCurve(
        math.log(3.0), 0.02, tuple(f - mean for f in raw)
    )
    t = [(k + 0.5) / 12 for k in range(30)]
    fit = commodity.fit_seasonal_curve(t, [truth.forward(v) for v in t])
    assert fit.level == pytest.approx(truth.level, abs=1e-10)
    assert fit.growth == pytest.approx(truth.growth, abs=1e-10)
    assert fit.factors == pytest.approx(truth.factors, abs=1e-10)
    with pytest.raises(ValueError):
        commodity.fit_seasonal_curve(t[:12], [1.0] * 12)


def test_spike_expectation_matches_monte_carlo() -> None:
    spiky = commodity.SpikeModel(math.log(40.0), 8.0, 0.8, 6.0, 0.8, 0.3)
    analytic = commodity.spike_expected_price(spiky, 0.0, 0.5)
    mc = commodity.spike_monte_carlo(spiky, 0.0, 0.5, 50, 40_000, 11)
    assert mc.mean == pytest.approx(analytic, abs=4.0 * mc.std_error)
    assert (
        len(commodity.simulate_spike_path(spiky, 0.0, 1 / 365, 365, 2)) == 366
    )
    with pytest.raises(ValueError):
        commodity.spike_expected_price(commodity.SpikeModel(), 0.0, 1.0)


def test_commodity_tree_fits_futures_and_parity() -> None:
    futures = [22.0, 23.0, 24.0, 23.5, 23.0, 24.5]
    tree = CommodityTree(20.0, futures, 0.1, 0.2, 0.5)
    for i in range(1, tree.steps + 1):
        w = tree.width(i)
        p = tree.probabilities(i)
        assert sum(p) == pytest.approx(1.0, abs=1e-12)
        expected = sum(p[j + w] * tree.price(i, j) for j in range(-w, w + 1))
        assert expected == pytest.approx(futures[i - 1], rel=1e-12)
    call = commodity_tree_option(
        tree, OptionType.CALL, Exercise.EUROPEAN, 23.0, 6, 0.05
    )
    put = commodity_tree_option(
        tree, OptionType.PUT, Exercise.EUROPEAN, 23.0, 6, 0.05
    )
    assert call - put == pytest.approx(math.exp(-0.15) * 1.5, abs=1e-12)
    with pytest.raises(ValueError):
        CommodityTree(20.0, futures, 0.0, 0.2, 0.5)


def test_degree_days_and_burn() -> None:
    temps = [60.0, 70.0, 65.0, 50.0]
    assert wi.degree_days(wi.DegreeDayKind.HEATING, temps) == 20.0
    assert wi.degree_days(wi.DegreeDayKind.COOLING, temps) == 5.0
    call = wi.DegreeDayOption(OptionType.CALL, 700.0, 10_000.0, 2e6)
    assert wi.degree_day_payoff(call, 1000.0) == 2e6
    burn = wi.burn_analysis(call, [650.0, 720.0, 800.0, 690.0], 0.05, 0.5)
    assert burn.value == pytest.approx(math.exp(-0.025) * 300_000.0)


def test_temperature_fit() -> None:
    truth = wi.TemperatureModel(55.0, 0.001, 20.0, 200.0, 0.7, 4.0)
    rng = np.random.default_rng(3)
    x = 0.0
    temps = []
    for d in range(365 * 20):
        x = truth.phi * x + truth.sigma * rng.standard_normal()
        temps.append(truth.seasonal(d) + x)
    fit = wi.fit_temperature_model(temps)
    assert fit.amplitude == pytest.approx(20.0, abs=0.3)
    assert fit.phase == pytest.approx(200.0, abs=2.0)
    assert fit.phi == pytest.approx(0.7, abs=0.03)
    assert fit.sigma == pytest.approx(4.0, abs=0.1)
    quiet = wi.TemperatureModel(55.0, 0.0, 20.0, 200.0, 0.7, 0.0)
    season = wi.SeasonSpec(wi.DegreeDayKind.HEATING, 334, 90)
    dd = wi.degree_days(
        wi.DegreeDayKind.HEATING, [quiet.seasonal(334 + d) for d in range(90)]
    )
    sim = wi.simulate_degree_day_option(
        quiet, season, wi.DegreeDayOption(strike=dd - 100.0), 0.0, 0.25, 10, 1
    )
    assert sim.option.value == pytest.approx(100.0, abs=1e-9)


def test_cat_bond_and_layer() -> None:
    bond = wi.CatBondTerms(100.0, 0.08, 3.0, 500.0, 1000.0, 2)
    riskless = 100.0 * math.exp(-0.09) + sum(
        4.0 * math.exp(-0.03 * k / 2) for k in range(1, 7)
    )
    safe = wi.price_cat_bond(bond, wi.LossModel(), 0.03, 100, 1)
    assert safe.price == pytest.approx(riskless, rel=1e-12)
    assert safe.spread == pytest.approx(0.0, abs=1e-9)
    hurricanes = wi.LossModel(0.3, math.log(300.0), 0.8)
    cat = wi.price_cat_bond(bond, hurricanes, 0.03, 100_000, 7)
    assert riskless - cat.price == pytest.approx(
        100.0 * math.exp(-0.09) * cat.expected_loss, abs=1e-9
    )
    assert cat.spread > 0.0
    fixed = wi.LossModel(2.0, math.log(10.0), 0.0)
    exact, prob = 0.0, math.exp(-2.0)
    for n in range(40):
        exact += prob * min(max(10.0 * n - 15.0, 0.0), 20.0)
        prob *= 2.0 / (n + 1)
    layer = wi.excess_of_loss_layer(fixed, 15.0, 20.0, 1.0, 0.0, 200_000, 5)
    assert layer.value == pytest.approx(exact, abs=4.0 * layer.std_error)


def test_market_price_of_risk() -> None:
    assert ro.market_price_of_risk(0.7, 0.10, 0.04, 0.20) == pytest.approx(0.21)
    assert ro.risk_neutral_growth(0.08, 0.21, 0.3) == pytest.approx(0.017)
    assert ro.risk_neutral_present_value(
        [1.0, 2.0], [100.0, 110.0], 0.2, 0.25, 0.05
    ) == pytest.approx(100.0 * math.exp(-0.1) + 110.0 * math.exp(-0.2))


def test_project_lattice_options() -> None:
    lattice = ro.ProjectLattice(100.0, 0.3, 0.05, 0.04, 2.0, 200)
    assert ro.project_with_options(lattice, []) == pytest.approx(100.0)
    put = VanillaOption(OptionType.PUT, Exercise.AMERICAN, 90.0, 2.0)
    market = binomial_tree.TreeMarket(100.0, 0.05, 0.04, 0.3)
    tree_put = binomial_tree.price_binomial(
        put, market, 200, binomial_tree.TreeScheme.COX_ROSS_RUBINSTEIN
    )
    assert ro.project_with_options(
        lattice, [ro.Abandon(90.0)]
    ) == pytest.approx(100.0 + tree_put.price, rel=1e-10)
    combined = ro.project_with_options(
        lattice,
        [ro.Abandon(90.0), ro.Expand(1.3, 25.0), ro.Contract(0.7, 25.0)],
    )
    assert combined >= 100.0 + tree_put.price
    call = VanillaOption(OptionType.CALL, Exercise.AMERICAN, 110.0, 2.0)
    assert ro.deferral_option(lattice, 110.0) == pytest.approx(
        binomial_tree.price_binomial(
            call, market, 200, binomial_tree.TreeScheme.COX_ROSS_RUBINSTEIN
        ).price,
        rel=1e-10,
    )


def test_extraction_project() -> None:
    futures = [22.0, 23.0, 24.0]
    tree = CommodityTree(20.0, futures, 0.1, 0.2, 1.0)
    v = ro.value_extraction_project(
        tree, ro.ExtractionProject(2.0, 17.0, 6.0, 0.1, 0.0, 1.2, 2.0)
    )
    pv = sum(
        math.exp(-0.1 * i) * (2.0 * (futures[i - 1] - 17.0) - 6.0)
        for i in range(1, 4)
    )
    assert v.base == pytest.approx(pv, abs=1e-12)
    assert v.with_abandon > v.base
    assert v.with_expand >= v.base


def test_business_value() -> None:
    spec = ro.BusinessSpec(50.0, 0.03, 0.0, 0.6, 15.0, 10.0, 0.04, 5.0, 4, 8.0)
    revenue, cash, flow = 50.0, 10.0, 0.0
    for _ in range(20):
        revenue *= math.exp(0.03 * 0.25)
        flow = revenue * 0.4 - 15.0
        cash = cash * math.exp(0.01) + flow * 0.25
    flat = ro.value_business(spec, 10, 1)
    assert flat.value == pytest.approx(math.exp(-0.2) * (cash + 8 * flow))
    with pytest.raises(ValueError):
        ro.value_business(ro.BusinessSpec(), 10, 1)
