"""VaR and ES (Ch 22), volatility models (Ch 23) and their numerics."""

import math

import pytest

from ofod import math as omath
from ofod import random
from ofod import risk
from ofod import stats


def test_eigen_reconstructs_matrix() -> None:
    a = [
        [4.0, 1.0, -2.0, 0.5],
        [1.0, 3.0, 0.0, 1.5],
        [-2.0, 0.0, 5.0, -1.0],
        [0.5, 1.5, -1.0, 2.0],
    ]
    e = omath.symmetric_eigen(a)
    assert e.values == sorted(e.values, reverse=True)
    for i in range(4):
        for j in range(4):
            total = sum(
                e.vectors[k][i] * e.values[k] * e.vectors[k][j]
                for k in range(4)
            )
            assert total == pytest.approx(a[i][j], abs=1e-12)


@pytest.mark.parametrize("bad", [[], [[1.0, 2.0]], [[1.0, 2.0], [0.0, 1.0]]])
def test_eigen_rejects_bad_input(bad: list[list[float]]) -> None:
    with pytest.raises(ValueError):
        omath.symmetric_eigen(bad)


def test_nelder_mead_rosenbrock() -> None:
    def f(x: list[float]) -> float:
        return 100 * (x[1] - x[0] ** 2) ** 2 + (1 - x[0]) ** 2

    m = omath.nelder_mead(f, [-1.2, 1.0], [0.5, 0.5])
    assert m.converged
    assert m.x == pytest.approx([1.0, 1.0], abs=1e-6)
    capped = omath.nelder_mead(
        f, [-1.2, 1.0], [0.5, 0.5], omath.NelderMeadOptions(max_iterations=5)
    )
    assert not capped.converged
    assert capped.iterations == 5


def test_garch_forecasts_revert() -> None:
    g = stats.Garch11(2e-6, 0.13, 0.86)
    assert g.expected_variance(4e-4, 0.0) == pytest.approx(4e-4, rel=1e-14)
    assert g.expected_variance(4e-4, 5000.0) == pytest.approx(
        g.long_run_variance(), rel=1e-12
    )
    assert g.term_structure_volatility(4e-4, 1e-6) == pytest.approx(
        math.sqrt(252 * 4e-4), rel=1e-6
    )


def test_garch_fit_beats_ewma_and_validates() -> None:
    rng = random.NormalGenerator(11)
    truth = stats.Garch11(2e-6, 0.10, 0.88)
    v, u = truth.long_run_variance(), []
    for _ in range(3000):
        x = math.sqrt(v) * rng.next()
        u.append(x)
        v = truth.update(v, x)
    fit = stats.fit_garch11(u)
    assert fit.alpha == pytest.approx(0.10, abs=0.04)
    assert fit.beta == pytest.approx(0.88, abs=0.04)
    assert fit.log_likelihood(u) >= truth.log_likelihood(u)
    assert fit.log_likelihood(u) > stats.ewma_log_likelihood(u, 0.94)
    with pytest.raises(ValueError):
        stats.fit_garch11([0.01] * 5)
    with pytest.raises(ValueError):
        stats.fit_garch11([0.0] * 50)


def test_ljung_box_white_noise() -> None:
    x = random.NormalGenerator(5).sample(2000)
    assert stats.ljung_box(x, 15) < 30.0
    with pytest.raises(ValueError):
        stats.ljung_box(x, 0)
    with pytest.raises(ValueError):
        stats.autocorrelation([1.0] * 10, 1)


def test_psd() -> None:
    assert stats.is_positive_semidefinite([[1.0, 0.3], [0.3, 1.0]])
    assert stats.is_positive_semidefinite([[1.0, 1.0], [1.0, 1.0]])
    assert not stats.is_positive_semidefinite([[1.0, 1.2], [1.2, 1.0]])
    assert not stats.is_positive_semidefinite([[1.0, 0.0]])


@pytest.mark.parametrize("x", [0.95, 0.975, 0.99])
def test_es_exceeds_var(x: float) -> None:
    assert risk.normal_expected_shortfall(1.0, x) > risk.normal_var(1.0, x)


def test_historical_needs_scenarios() -> None:
    with pytest.raises(ValueError):
        risk.historical_var([1.0, 2.0, 3.0], 0.99)
    assert risk.historical_var([1.0, 2.0, 3.0], 0.5) == 2.0


def test_delta_gamma_reduces_to_normal() -> None:
    m = risk.delta_gamma_moments(50.0, 0.0, 0.02)
    assert m.std_dev == pytest.approx(1.0, abs=1e-12)
    assert risk.cornish_fisher_var(m, 0.99) == pytest.approx(
        risk.normal_var(1.0, 0.99), abs=1e-12
    )


def test_monte_carlo_var_matches_linear_model() -> None:
    cov = [[4e-4, 6e-5], [6e-5, 1e-4]]
    mc = risk.monte_carlo_var(
        lambda dx: 10e6 * dx[0] + 5e6 * dx[1], cov, 0.99, 50000, 3
    )
    assert mc.var == pytest.approx(risk.normal_var(220227.0, 0.99), rel=0.04)
    with pytest.raises(ValueError):
        risk.monte_carlo_var(lambda dx: 0.0, cov, 0.99, 10, 1)


def test_kupiec_and_binomial_edges() -> None:
    assert risk.kupiec_statistic(1000, 10, 0.01) == pytest.approx(0, abs=1e-12)
    assert risk.binomial_tail_probability(100, 0, 0.01) == 1.0
    assert risk.binomial_tail_probability(10, 11, 0.5) == 0.0
    assert risk.binomial_tail_probability(10, 6, 0.5) == pytest.approx(
        386 / 1024, abs=1e-12
    )


def test_pca_one_factor() -> None:
    rng = random.NormalGenerator(9)
    rows = []
    for _ in range(400):
        f = rng.next()
        rows.append([f, 2 * f, -f])
    pca = risk.principal_components(rows)
    assert pca.variances[1] < 1e-12 * pca.variances[0]
    assert abs(pca.loadings[0][1]) == pytest.approx(2 / math.sqrt(6), abs=1e-10)
    with pytest.raises(ValueError):
        risk.principal_components([[1.0]])
    with pytest.raises(ValueError):
        risk.pca_std_dev([1.0], [[1.0, 2.0]], [1.0])
