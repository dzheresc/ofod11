"""Parity of the Ch 35-36 bindings with the pure-Python package."""

import math

import numpy as np
import pytest

import ofod
import ofod_cpp

_LIBS = (ofod, ofod_cpp)


def _tree(lib):
    return lib.pricers.CommodityTree(
        20.0, [22.0, 23.0, 24.0, 23.0], 0.1, 0.2, 0.5
    )


def test_hull_branch() -> None:
    for j in (-3, 0, 3):
        py = ofod.pricers.hull_branch(j, 3, -0.06)
        cpp = ofod_cpp.pricers.hull_branch(j, 3, -0.06)
        assert (cpp.middle, cpp.up, cpp.mid, cpp.down) == pytest.approx(
            (py.middle, py.up, py.mid, py.down), abs=1e-15
        )


@pytest.mark.parametrize("ptype", ["CALL", "PUT"])
@pytest.mark.parametrize("ex", ["EUROPEAN", "AMERICAN"])
def test_commodity_tree(ptype: str, ex: str) -> None:
    values = []
    for lib in _LIBS:
        tree = _tree(lib)
        values.append(
            (
                tree.alphas,
                tree.probabilities(4),
                lib.pricers.commodity_tree_option(
                    tree,
                    getattr(lib.instruments.OptionType, ptype),
                    getattr(lib.instruments.Exercise, ex),
                    22.5,
                    4,
                    0.05,
                ),
            )
        )
    for a, b in zip(values[0], values[1], strict=True):
        assert b == pytest.approx(a, abs=1e-13)
    with pytest.raises(ValueError):
        ofod_cpp.pricers.CommodityTree(20.0, [], 0.1, 0.2, 0.5)


def test_seasonal_and_spike() -> None:
    t = [(k + 0.5) / 12 for k in range(20)]
    f = [
        2.0 + 0.3 * math.cos(2 * math.pi * v) + 0.01 * k
        for k, v in enumerate(t)
    ]
    py = ofod.models.fit_seasonal_curve(t, f)
    cpp = ofod_cpp.models.fit_seasonal_curve(t, f)
    assert cpp.level == pytest.approx(py.level, abs=1e-11)
    assert list(cpp.factors) == pytest.approx(list(py.factors), abs=1e-11)
    assert cpp.forward(0.7) == pytest.approx(py.forward(0.7), rel=1e-12)
    args = (math.log(40.0), 8.0, 0.8, 6.0, 0.8, 0.3)
    assert ofod_cpp.models.spike_expected_price(
        ofod_cpp.models.SpikeModel(*args), 0.1, 0.3
    ) == pytest.approx(
        ofod.models.spike_expected_price(
            ofod.models.SpikeModel(*args), 0.1, 0.3
        ),
        rel=1e-12,
    )
    mc = ofod_cpp.models.spike_monte_carlo(
        ofod_cpp.models.SpikeModel(*args), 0.0, 0.5, 50, 20_000, 1
    )
    exact = ofod.models.spike_expected_price(
        ofod.models.SpikeModel(*args), 0.0, 0.5
    )
    assert mc.mean == pytest.approx(exact, abs=4.0 * mc.std_error)


def test_weather() -> None:
    rng = np.random.default_rng(1)
    temps = [
        50 + 20 * math.cos(2 * math.pi * (d - 200) / 365) + rng.normal(0, 3)
        for d in range(800)
    ]
    fits = [lib.pricers.fit_temperature_model(temps, 10) for lib in _LIBS]
    for name in ("mean", "trend", "amplitude", "phase", "phi", "sigma"):
        assert getattr(fits[1], name) == pytest.approx(
            getattr(fits[0], name), rel=1e-9, abs=1e-12
        )
    seasons = [650.0, 720.0, 800.0]
    burns = []
    for lib in _LIBS:
        opt = lib.pricers.DegreeDayOption(
            lib.instruments.OptionType.PUT, 720.0, 5000.0, 1e5
        )
        burns.append(lib.pricers.burn_analysis(opt, seasons, 0.04, 0.5).value)
        assert lib.pricers.degree_days(
            lib.pricers.DegreeDayKind.COOLING, temps[150:240]
        ) == pytest.approx(
            ofod.pricers.degree_days(
                ofod.pricers.DegreeDayKind.COOLING, temps[150:240]
            )
        )
    assert burns[1] == pytest.approx(burns[0], rel=1e-13)


def test_insurance_statistically() -> None:
    losses = (0.3, math.log(300.0), 0.8)
    out = []
    for lib in _LIBS:
        terms = lib.pricers.CatBondTerms(100.0, 0.08, 3.0, 500.0, 1000.0, 2)
        out.append(
            lib.pricers.price_cat_bond(
                terms, lib.pricers.LossModel(*losses), 0.03, 50_000, 3
            )
        )
    se = math.hypot(out[0].std_error, out[1].std_error)
    assert out[1].price == pytest.approx(out[0].price, abs=4.0 * se)


def test_real_options() -> None:
    results = []
    for lib in _LIBS:
        p = lib.pricers
        lattice = p.ProjectLattice(100.0, 0.3, 0.05, 0.04, 2.0, 100)
        opts = [p.Abandon(90.0), p.Expand(1.3, 25.0), p.Contract(0.7, 25.0)]
        project = p.ExtractionProject(2.0, 17.0, 6.0, 0.1, 0.0, 1.2, 2.0)
        v = p.value_extraction_project(_tree(lib), project)
        spec = p.BusinessSpec(
            50.0, 0.03, 0.0, 0.6, 15.0, 10.0, 0.04, 5.0, 4, 8.0
        )
        results.append(
            [
                p.project_with_options(lattice, opts),
                p.deferral_option(lattice, 110.0),
                v.base,
                v.with_abandon,
                v.with_expand,
                p.exercise_value(p.Expand(1.2, 3.0), 50.0),
                p.risk_neutral_present_value(
                    [1.0, 2.0], [5.0, 6.0], 0.2, 0.3, 0.04
                ),
                p.value_business(spec, 4, 1).value,
            ]
        )
    assert results[1] == pytest.approx(results[0], rel=1e-11)
    with pytest.raises(ValueError):
        ofod_cpp.pricers.project_with_options(
            ofod_cpp.pricers.ProjectLattice(), []
        )
