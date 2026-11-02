"""Checks that ofod_cpp and ofod expose the same API and agree numerically."""

import datetime
import math

from hypothesis import given
from hypothesis import strategies as st
import pytest

import ofod
import ofod_cpp

_SUBMODULES = (
    "core",
    "credit",
    "curves",
    "futures",
    "greeks",
    "instruments",
    "math",
    "models",
    "pricers",
    "random",
    "risk",
    "stats",
    "strategies",
)
# Typing-only names (Protocols) have no runtime counterpart in C++.
_TYPING_ONLY = {"Payoff"}


@pytest.mark.parametrize("name", _SUBMODULES)
def test_same_public_names(name: str) -> None:
    py_names = set(getattr(ofod, name).__all__)
    cpp_names = {
        n for n in dir(getattr(ofod_cpp, name)) if not n.startswith("_")
    }
    missing = py_names - cpp_names - _TYPING_ONLY
    assert not missing, f"ofod_cpp.{name} lacks {sorted(missing)}"


_rates = st.floats(min_value=-0.02, max_value=0.25)
_times = st.floats(min_value=0.05, max_value=30.0)
_freqs = st.sampled_from([0, 1, 2, 4, 12])


def _comp(lib, m: int):
    c = lib.core.Compounding
    return c.continuous() if m == 0 else c.periodic(m)


@given(_rates, _times, _freqs)
def test_growth_factor(rate: float, t: float, m: int) -> None:
    py = ofod.core.growth_factor(rate, t, _comp(ofod, m))
    cpp = ofod_cpp.core.growth_factor(rate, t, _comp(ofod_cpp, m))
    assert cpp == pytest.approx(py, rel=1e-13)


@given(
    st.floats(min_value=0.0, max_value=0.15),
    st.floats(min_value=0.5, max_value=30.0),
    st.sampled_from([1, 2, 4]),
    _rates,
)
def test_bond_price_and_duration(
    coupon: float, maturity: float, freq: int, y: float
) -> None:
    py_bond = ofod.instruments.Bond(
        coupon_rate=coupon, maturity=maturity, frequency=freq
    )
    cpp_bond = ofod_cpp.instruments.Bond(
        coupon_rate=coupon, maturity=maturity, frequency=freq
    )
    assert ofod_cpp.pricers.bond_price_from_yield(cpp_bond, y) == pytest.approx(
        ofod.pricers.bond_price_from_yield(py_bond, y), rel=1e-12
    )
    assert ofod_cpp.pricers.macaulay_duration(cpp_bond, y) == pytest.approx(
        ofod.pricers.macaulay_duration(py_bond, y), rel=1e-12
    )


@given(st.lists(_rates, min_size=1, max_size=8), _times)
def test_zero_curve(rates: list[float], t: float) -> None:
    times = [0.5 * (i + 1) for i in range(len(rates))]
    py = ofod.curves.ZeroCurve(times, rates)
    cpp = ofod_cpp.curves.ZeroCurve(times, rates)
    assert cpp.discount_factor(t) == pytest.approx(
        py.discount_factor(t), rel=1e-13
    )


def test_errors_match() -> None:
    for lib in (ofod, ofod_cpp):
        with pytest.raises(ValueError):
            lib.curves.ZeroCurve([1.0, 1.0], [0.01, 0.02])


def test_year_fraction_accepts_python_dates() -> None:
    a, b = datetime.date(2023, 7, 1), datetime.date(2024, 7, 1)
    for convention in ("ACTUAL_360", "THIRTY_360", "ACTUAL_ACTUAL_ISDA"):
        py = ofod.core.year_fraction(
            a, b, getattr(ofod.core.DayCount, convention)
        )
        cpp = ofod_cpp.core.year_fraction(
            a, b, getattr(ofod_cpp.core.DayCount, convention)
        )
        assert cpp == pytest.approx(py, abs=1e-15)


def test_positions_accept_any_callable() -> None:
    pos = ofod_cpp.instruments.Position(
        ofod_cpp.instruments.CallPayoff(340.0), quantity=100.0, premium=5.0
    )
    assert pos.profit_at(400.0) == 5500.0
    straddle_leg = ofod_cpp.instruments.Position(lambda s: abs(s - 100.0))
    assert straddle_leg.payoff_at(90.0) == 10.0


@given(
    st.floats(min_value=20.0, max_value=80.0),
    st.floats(min_value=20.0, max_value=80.0),
    st.floats(min_value=0.05, max_value=0.6),
    st.sampled_from(["COX_ROSS_RUBINSTEIN", "JARROW_RUDD", "TIAN"]),
    st.sampled_from(["EUROPEAN", "AMERICAN"]),
)
def test_binomial_tree(
    spot: float, strike: float, vol: float, scheme: str, exercise: str
) -> None:
    def price(lib) -> float:
        option = lib.instruments.VanillaOption(
            lib.instruments.OptionType.PUT,
            getattr(lib.instruments.Exercise, exercise),
            strike,
            1.0,
        )
        market = lib.pricers.TreeMarket(spot, 0.05, 0.01, vol)
        scheme_ = getattr(lib.pricers.TreeScheme, scheme)
        return lib.pricers.price_binomial(option, market, 50, scheme_).price

    assert price(ofod_cpp) == pytest.approx(price(ofod), rel=1e-11, abs=1e-12)


def test_portfolio_accepts_python_payoffs() -> None:
    p = ofod_cpp.instruments.Portfolio()
    p.add(1.0, ofod_cpp.instruments.CallPayoff(50.0), 3.0).add(
        -1.0, lambda s: max(s - 60.0, 0.0), 1.0
    )
    assert p.profit_at(70.0) == 8.0
    assert len(p.legs) == 2
    for lib in (ofod, ofod_cpp):
        with pytest.raises(ValueError):
            lib.strategies.replicate_payoff(0.0, [1.0], [40.0])


@given(
    st.sampled_from(["CALL", "PUT"]),
    st.floats(min_value=20.0, max_value=80.0),
    st.floats(min_value=20.0, max_value=80.0),
    st.floats(min_value=0.05, max_value=1.0),
    st.floats(min_value=0.05, max_value=3.0),
)
def test_bsm_price(
    kind: str, spot: float, strike: float, vol: float, t: float
) -> None:
    def price(lib) -> float:
        option_type = getattr(lib.instruments.OptionType, kind)
        return lib.pricers.bsm_price(
            option_type, spot, strike, 0.04, 0.01, vol, t
        )

    assert price(ofod_cpp) == pytest.approx(price(ofod), rel=1e-12, abs=1e-13)


def test_gbm_paths_agree() -> None:
    shocks = ofod.random.NormalGenerator(4).sample(50)
    py = ofod.models.GeometricBrownianMotion(0.1, 0.3)
    cpp = ofod_cpp.models.GeometricBrownianMotion(0.1, 0.3)
    for method in ("euler_path", "exact_path"):
        a = getattr(py, method)(100.0, 0.01, shocks)
        b = getattr(cpp, method)(100.0, 0.01, shocks)
        assert b == pytest.approx(a, rel=1e-13)


def test_sobol_points_identical() -> None:
    a, b = ofod.random.Sobol(8), ofod_cpp.random.Sobol(8)
    for _ in range(300):
        assert a.next() == b.next()


@given(
    st.sampled_from(["CALL", "PUT"]),
    st.sampled_from(["EUROPEAN", "AMERICAN"]),
    st.floats(min_value=30.0, max_value=70.0),
)
def test_trees_and_finite_differences_agree(
    kind: str, exercise: str, strike: float
) -> None:
    def run(lib) -> list[float]:
        ins, p = lib.instruments, lib.pricers
        option = ins.VanillaOption(
            getattr(ins.OptionType, kind),
            getattr(ins.Exercise, exercise),
            strike,
            0.75,
        )
        market = p.TreeMarket(50.0, 0.06, 0.0, 0.3)
        grid = p.FdGrid(0.0, 200.0, 80, 40)
        divs = [ins.CashFlow(0.3, 1.0)]
        return [
            p.price_trinomial(option, market, 40).price,
            p.price_binomial_with_dividends(option, market, 40, divs).price,
            p.price_finite_difference(
                option, market, grid, p.FdScheme.CRANK_NICOLSON
            ),
            p.price_finite_difference(
                option, market, grid, p.FdScheme.EXPLICIT
            ),
        ]

    assert run(ofod_cpp) == pytest.approx(run(ofod), rel=1e-10, abs=1e-10)


def test_monte_carlo_agrees_statistically() -> None:
    option = ofod.instruments.VanillaOption(strike=40.0, maturity=0.5)
    for name in ("NONE", "ANTITHETIC", "CONTROL_VARIATE", "QUASI_RANDOM"):
        results = []
        for lib in (ofod, ofod_cpp):
            p = lib.pricers
            opt = lib.instruments.VanillaOption(strike=40.0, maturity=0.5)
            market = p.TreeMarket(42.0, 0.1, 0.0, 0.2)
            options = p.McOptions(
                paths=40_000, method=getattr(p.VarianceReduction, name)
            )
            results.append(p.monte_carlo_european(opt, market, options))
        a, b = results
        assert abs(a.price - b.price) < 5 * math.hypot(a.std_error, b.std_error)
    assert option.strike == 40.0
    assert ofod.pricers.mix_seed(7, 3) == ofod_cpp.pricers.mix_seed(7, 3)


def test_path_simulator_accepts_python_payoff() -> None:
    p = ofod_cpp.pricers
    r = p.simulate_gbm_paths(
        p.TreeMarket(42.0, 0.1, 0.0, 0.2),
        0.5,
        4,
        lambda path: max(sum(path[1:]) / 4 - 40.0, 0.0),
        p.McOptions(paths=5000, threads=4),
    )
    assert r.paths == 5000
    assert r.price > 0.0


# Chapter 19, 20 and 16 parity.


def _bsm_pricer(lib, strike: float):
    call = lib.instruments.OptionType.CALL

    def pricer(x) -> float:
        return lib.pricers.bsm_price(
            call, x.spot, strike, x.rate, x.yield_, x.volatility, x.maturity
        )

    return pricer


@given(
    st.floats(min_value=20.0, max_value=80.0),
    st.floats(min_value=0.05, max_value=0.6),
    st.floats(min_value=0.05, max_value=3.0),
)
def test_greeks_agree(spot: float, vol: float, t: float) -> None:
    py_in = ofod.greeks.OptionInputs(spot, 0.04, 0.01, vol, t)
    cpp_in = ofod_cpp.greeks.OptionInputs(spot, 0.04, 0.01, vol, t)
    call_py = ofod.instruments.OptionType.CALL
    call_cpp = ofod_cpp.instruments.OptionType.CALL
    a = ofod.greeks.bsm_greeks(call_py, 50.0, py_in)
    b = ofod_cpp.greeks.bsm_greeks(call_cpp, 50.0, cpp_in)
    for name in ("delta", "gamma", "vega", "theta", "rho"):
        assert getattr(b, name) == pytest.approx(
            getattr(a, name), rel=1e-11, abs=1e-12
        )
    bumped_py = ofod.greeks.bump_greeks(_bsm_pricer(ofod, 50.0), py_in)
    bumped = ofod_cpp.greeks.bump_greeks(_bsm_pricer(ofod_cpp, 50.0), cpp_in)
    assert bumped.delta == pytest.approx(bumped_py.delta, rel=1e-7, abs=1e-9)


def test_hedges_and_scenarios_agree() -> None:
    for lib in (ofod, ofod_cpp):
        g = lib.greeks
        h = g.gamma_vega_neutral_hedge(
            g.Exposure(0.0, -5000.0, -8000.0),
            g.Exposure(0.6, 0.5, 2.0),
            g.Exposure(0.5, 0.8, 1.2),
        )
        assert (h.option1, h.option2, h.underlying) == pytest.approx(
            (400.0, 6000.0, -3240.0)
        )
        with pytest.raises(ValueError):
            g.gamma_neutral_hedge(g.Exposure(), g.Exposure(delta=1.0))
    base_py = ofod.greeks.OptionInputs(49.0, 0.05, 0.0, 0.2, 0.4)
    base_cpp = ofod_cpp.greeks.OptionInputs(49.0, 0.05, 0.0, 0.2, 0.4)
    spots, vols = [45.0, 49.0, 53.0], [0.15, 0.2, 0.25]
    a = ofod.greeks.scenario_grid(_bsm_pricer(ofod, 50.0), base_py, spots, vols)
    b = ofod_cpp.greeks.scenario_grid(
        _bsm_pricer(ofod_cpp, 50.0), base_cpp, spots, vols
    )
    for ra, rb in zip(a, b, strict=True):
        assert rb == pytest.approx(ra, abs=1e-12)


def test_delta_hedge_agrees_statistically() -> None:
    results = []
    for lib in (ofod, ofod_cpp):
        e = lib.greeks.HedgeExperiment(49.0, 50.0, 0.05, 0.13, 0.2, 20 / 52)
        results.append(lib.greeks.simulate_delta_hedge(e, 20, 20000, 5))
    py, cpp = results
    assert cpp.bsm_price == pytest.approx(py.bsm_price, rel=1e-13)
    assert cpp.ratio == pytest.approx(py.ratio, abs=0.01)
    with pytest.raises(ValueError):
        ofod_cpp.greeks.simulate_delta_hedge(
            ofod_cpp.greeks.HedgeExperiment(), 0, 10, 1
        )


@given(
    st.floats(min_value=50.0, max_value=150.0),
    st.floats(min_value=0.01, max_value=8.0),
)
def test_vol_surface_agrees(strike: float, t: float) -> None:
    grid = ([0.5, 1.0, 2.0], [80.0, 100.0, 120.0])
    vols = [[0.3, 0.2, 0.25], [0.28, 0.21, 0.24], [0.26, 0.22, 0.23]]
    for interp in ("TOTAL_VARIANCE", "LINEAR_VOL"):
        a = ofod.curves.VolSurface(
            *grid, vols, getattr(ofod.curves.VolInterpolation, interp)
        )
        b = ofod_cpp.curves.VolSurface(
            *grid, vols, getattr(ofod_cpp.curves.VolInterpolation, interp)
        )
        assert b.volatility(strike, t) == pytest.approx(
            a.volatility(strike, t), rel=1e-14
        )
    with pytest.raises(ValueError):
        ofod_cpp.curves.VolSurface([1.0], [100.0], [[0.0]])


def test_smile_tools_agree() -> None:
    results = []
    for lib in (ofod, ofod_cpp):
        p = lib.pricers
        call = lib.instruments.OptionType.CALL
        results.append(
            (
                p.strike_from_delta(call, 0.25, 1.0, 0.02, 0.01, 0.1, 0.5),
                p.minimum_variance_delta(0.5, 10.0, 50.0, 0.5, -0.2, 0.3, -0.4),
                p.implied_density(1.0, 0.9, 0.82, 0.1, 0.03, 0.5),
                p.lognormal_density(10.0, 9.0, 0.03, 0.0, 0.3, 0.25),
                p.binary_jump_price(call, 50.0, 0.01, 0.1, 58.0, 42.0, 0.4),
            )
        )
        with pytest.raises(ValueError):
            p.strike_from_delta(call, 1.5, 1.0, 0.02, 0.01, 0.1, 0.5)
    assert results[1] == pytest.approx(results[0], rel=1e-13)


def test_eso_agrees() -> None:
    values = []
    for lib in (ofod, ofod_cpp):
        p = lib.pricers
        market = p.TreeMarket(spot=40.0, rate=0.05, yield_=0.0, volatility=0.3)
        terms = p.EsoTerms(40.0, 10.0, 3.0, 1.5, 0.04)
        values.append(p.price_employee_stock_option(terms, market, 300))
        with pytest.raises(ValueError):
            p.price_employee_stock_option(
                p.EsoTerms(40.0, 10.0, 3.0, 0.5, 0.04), market, 300
            )
        values.append(p.expected_life_value(40.0, 40.0, 0.05, 0.0, 0.3, 6.5))
    assert values[2] == pytest.approx(values[0], rel=1e-11)
    assert values[3] == pytest.approx(values[1], rel=1e-14)


# Chapter 22 and 23 parity.


def _garch_sample(n: int = 800) -> list[float]:
    rng = ofod.random.NormalGenerator(21)
    g = ofod.stats.Garch11(2e-6, 0.12, 0.86)
    v, u = g.long_run_variance(), []
    for _ in range(n):
        x = math.sqrt(v) * rng.next()
        u.append(x)
        v = g.update(v, x)
    return u


@given(
    st.lists(st.floats(min_value=-5.0, max_value=5.0), min_size=10, max_size=10)
)
def test_symmetric_eigen_identical(entries: list[float]) -> None:
    a = [[0.0] * 4 for _ in range(4)]
    k = 0
    for i in range(4):
        for j in range(i, 4):
            a[i][j] = a[j][i] = entries[k]
            k += 1
    py = ofod.math.symmetric_eigen(a)
    cpp = ofod_cpp.math.symmetric_eigen(a)
    assert cpp.values == pytest.approx(py.values, abs=1e-12)
    for vp, vc in zip(py.vectors, cpp.vectors, strict=True):
        assert vc == pytest.approx(vp, abs=1e-9)


def test_nelder_mead_identical() -> None:
    def f(x) -> float:
        return (x[0] - 1.0) ** 2 + 10.0 * (x[1] + 2.0) ** 2 + x[0] * x[1]

    a = ofod.math.nelder_mead(f, [0.0, 0.0], [0.5, 0.5])
    b = ofod_cpp.math.nelder_mead(f, [0.0, 0.0], [0.5, 0.5])
    assert b.iterations == a.iterations
    assert b.x == pytest.approx(a.x, rel=1e-14, abs=1e-15)
    with pytest.raises(ValueError):
        ofod_cpp.math.nelder_mead(f, [0.0, 0.0], [0.5])


def test_garch_identical() -> None:
    u = _garch_sample()
    a = ofod.stats.fit_garch11(u)
    b = ofod_cpp.stats.fit_garch11(u)
    assert (b.omega, b.alpha, b.beta) == pytest.approx(
        (a.omega, a.alpha, a.beta), rel=1e-9
    )
    assert b.log_likelihood(u) == pytest.approx(a.log_likelihood(u), rel=1e-13)
    assert ofod_cpp.stats.ewma_log_likelihood(u, 0.94) == pytest.approx(
        ofod.stats.ewma_log_likelihood(u, 0.94), rel=1e-13
    )
    sq = [x * x for x in u]
    assert ofod_cpp.stats.ljung_box(sq, 10) == pytest.approx(
        ofod.stats.ljung_box(sq, 10), rel=1e-12
    )
    assert ofod_cpp.stats.ewma_variances(u[:5], 0.9, 1e-4) == pytest.approx(
        ofod.stats.ewma_variances(u[:5], 0.9, 1e-4), rel=1e-15
    )
    with pytest.raises(ValueError):
        ofod_cpp.stats.fit_garch11([0.01] * 5)


@given(st.floats(min_value=0.9, max_value=0.999))
def test_var_functions_identical(x: float) -> None:
    losses = [float((i * 37) % 101) for i in range(500)]
    for name, args in (
        ("normal_var", (2.0, x, 0.1)),
        ("normal_expected_shortfall", (2.0, x, 0.1)),
        ("historical_var", (losses, x)),
        ("historical_expected_shortfall", (losses, x)),
        ("kupiec_statistic", (750, 12, 1 - x)),
        ("binomial_tail_probability", (750, 12, 1 - x)),
    ):
        a = getattr(ofod.risk, name)(*args)
        b = getattr(ofod_cpp.risk, name)(*args)
        assert b == pytest.approx(a, rel=1e-12), name
    m_py = ofod.risk.delta_gamma_moments(100.0, -50.0, 0.5)
    m_cpp = ofod_cpp.risk.delta_gamma_moments(100.0, -50.0, 0.5)
    assert ofod_cpp.risk.cornish_fisher_var(m_cpp, x) == pytest.approx(
        ofod.risk.cornish_fisher_var(m_py, x), rel=1e-13
    )


def test_pca_and_mc_var_agree() -> None:
    rng = ofod.random.NormalGenerator(4)
    rows = []
    for _ in range(300):
        f, g = rng.next(), rng.next()
        rows.append([f + 0.1 * g, f, f - 0.1 * g, 0.01 * rng.next()])
    a = ofod.risk.principal_components(rows)
    b = ofod_cpp.risk.principal_components(rows)
    assert b.variances == pytest.approx(a.variances, rel=1e-11)
    assert b.loadings[0] == pytest.approx(a.loadings[0], abs=1e-10)
    cov = [[4e-4, 6e-5], [6e-5, 1e-4]]
    results = [
        lib.risk.monte_carlo_var(
            lambda dx: 10e6 * dx[0] + 5e6 * dx[1], cov, 0.99, 40000, 2
        )
        for lib in (ofod, ofod_cpp)
    ]
    assert results[1].var == pytest.approx(results[0].var, rel=0.05)
    with pytest.raises(ValueError):
        ofod_cpp.risk.monte_carlo_var(
            lambda dx: 0.0, [[1.0, 2.0], [2.0, 1.0]], 0.99, 1000, 1
        )


@given(
    a=st.floats(-4.0, 4.0),
    b=st.floats(-4.0, 4.0),
    rho=st.floats(-0.999, 0.999),
)
def test_bivariate_normal_identical(a: float, b: float, rho: float) -> None:
    assert ofod_cpp.math.bivariate_norm_cdf(a, b, rho) == pytest.approx(
        ofod.math.bivariate_norm_cdf(a, b, rho), rel=1e-12, abs=1e-15
    )


def test_quadrature_and_credit_curve_identical() -> None:
    a, b = (
        ofod.math.gauss_hermite_normal(32),
        ofod_cpp.math.gauss_hermite_normal(32),
    )
    assert b.nodes == pytest.approx(a.nodes, rel=1e-13, abs=1e-14)
    assert b.weights == pytest.approx(a.weights, rel=1e-11, abs=1e-30)
    args = ([1.0, 3.0, 5.0], [0.01, 0.02, 0.015])
    py, cpp = ofod.curves.CreditCurve(*args), ofod_cpp.curves.CreditCurve(*args)
    for t in (0.5, 2.0, 4.0, 8.0):
        assert cpp.survival_probability(t) == pytest.approx(
            py.survival_probability(t), rel=1e-15
        )
        assert cpp.default_probability(t) == pytest.approx(
            py.default_probability(t), rel=1e-13
        )
        assert cpp.default_probability(t, t + 1) == pytest.approx(
            py.default_probability(t, t + 1), rel=1e-13
        )
    with pytest.raises(ValueError):
        ofod_cpp.curves.CreditCurve([2.0, 1.0], [0.01, 0.01])
    with pytest.raises(ValueError):
        ofod_cpp.math.gauss_hermite(0)


def test_default_models_identical() -> None:
    a = ofod.credit.merton_solve(3.0, 0.8, 10.0, 0.05, 1.0)
    b = ofod_cpp.credit.merton_solve(3.0, 0.8, 10.0, 0.05, 1.0)
    assert b.asset_value == pytest.approx(a.asset_value, rel=1e-10)
    assert b.default_probability == pytest.approx(
        a.default_probability, rel=1e-9
    )
    for lib in (ofod, ofod_cpp):
        with pytest.raises(ValueError):
            lib.credit.merton_solve(3.0, 0.8, -10.0, 0.05, 1.0)
    for f, args in [
        ("wcdr", (0.02, 0.1, 0.999)),
        ("vasicek_credit_loss", (100.0, 0.02, 0.4, 0.1, 0.999)),
        ("joint_default_probability", (0.01, 0.02, 0.3)),
        ("conditional_default_probability", (0.03, 0.2, -1.5)),
    ]:
        assert getattr(ofod_cpp.credit, f)(*args) == pytest.approx(
            getattr(ofod.credit, f)(*args), rel=1e-12
        )


def test_cds_identical() -> None:
    mats, spreads = [1.0, 3.0, 5.0], [0.006, 0.009, 0.012]
    curves = [
        lib.credit.bootstrap_credit_curve(
            mats, spreads, 4, 0.4, lib.curves.ZeroCurve.flat(0.04)
        )
        for lib in (ofod, ofod_cpp)
    ]
    assert curves[1].hazards == pytest.approx(curves[0].hazards, rel=1e-10)
    values = [
        lib.credit.cds_upfront(
            lib.credit.CdsTerms(5.0, 4, 0.4),
            0.01,
            lib.curves.CreditCurve.flat(0.03),
            lib.curves.ZeroCurve.flat(0.04),
        )
        for lib in (ofod, ofod_cpp)
    ]
    assert values[1] == pytest.approx(values[0], rel=1e-13)
    assert ofod_cpp.credit.cds_option_price(
        False, 0.012, 0.01, 0.5, 1.0, 3.5
    ) == pytest.approx(
        ofod.credit.cds_option_price(False, 0.012, 0.01, 0.5, 1.0, 3.5)
    )
    with pytest.raises(ValueError):
        ofod_cpp.credit.cds_par_spread(
            ofod_cpp.credit.CdsTerms(0.0),
            ofod_cpp.curves.CreditCurve.flat(0.01),
            ofod_cpp.curves.ZeroCurve.flat(0.01),
        )


def test_copula_identical() -> None:
    p = [0.1, 0.2, 0.05, 0.4]
    assert ofod_cpp.credit.default_count_distribution(p) == pytest.approx(
        ofod.credit.default_count_distribution(p), rel=1e-14
    )
    spreads, rhos = [], []
    for lib in (ofod, ofod_cpp):
        c = lib.credit
        t, pool = c.Tranche(0.03, 0.07), c.CdoPool(100, 0.012, 0.4)
        terms, z = c.CdoTerms(5.0, 4, 32), lib.curves.ZeroCurve.flat(0.03)
        spreads.append(c.tranche_spread(t, pool, 0.25, terms, z))
        rhos.append(c.implied_tranche_correlation(t, pool, 0.02, terms, z))
    assert spreads[1] == pytest.approx(spreads[0], rel=1e-10)
    assert rhos[1] == pytest.approx(rhos[0], abs=1e-9)
    results = [
        lib.credit.nth_to_default_spread(
            lib.credit.BasketTerms(), 0.05, 40000, 5
        )
        for lib in (ofod, ofod_cpp)
    ]
    gap = abs(results[1].spread - results[0].spread)
    assert gap < 4.0 * math.hypot(results[0].std_error, results[1].std_error)
    with pytest.raises(ValueError):
        ofod_cpp.credit.nth_to_default_spread(
            ofod_cpp.credit.BasketTerms(nth=0), 0.05, 100, 1
        )


def test_xva_identical() -> None:
    values = [[1.0, -2.0, 3.0], [3.0, 4.0, -1.0], [-1.0, 10.0, 2.0]]
    times = [0.5, 1.0, 1.5]
    a = ofod.credit.profile_from_paths(times, values, 0.9, 5.0)
    b = ofod_cpp.credit.profile_from_paths(times, values, 0.9, 5.0)
    assert b.expected_exposure == pytest.approx(a.expected_exposure)
    assert b.expected_negative_exposure == pytest.approx(
        a.expected_negative_exposure
    )
    assert b.potential_future_exposure == pytest.approx(
        a.potential_future_exposure
    )
    sims = [
        lib.credit.simulate_forward_exposure(
            100.0, 100.0, 0.03, 0.25, 1.0, 4, 40000, 9
        )
        for lib in (ofod, ofod_cpp)
    ]
    assert sims[1].expected_exposure[-1] == pytest.approx(
        sims[0].expected_exposure[-1], rel=0.03
    )
    for lib in (ofod, ofod_cpp):
        curve = lib.curves.CreditCurve.flat(0.02)
        b_cva = lib.credit.cva(times, [1.0, 2.0, 3.0], curve, 0.4)
        assert b_cva == pytest.approx(
            ofod.credit.cva(
                times, [1.0, 2.0, 3.0], ofod.curves.CreditCurve.flat(0.02), 0.4
            ),
            rel=1e-14,
        )
        assert lib.credit.funding_value_adjustment(
            times, [1.0, 2.0, 3.0], 0.01
        ) == pytest.approx(0.03)
    with pytest.raises(ValueError):
        ofod_cpp.credit.cva(
            [1.0], [1.0, 2.0], ofod_cpp.curves.CreditCurve.flat(0.01), 0.4
        )
