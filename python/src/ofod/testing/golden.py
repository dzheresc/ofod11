"""Runs the book's golden cases (data/golden/*.json) against a library.

`lib` is any module that exposes the ofod API: `ofod` itself, or the C++
bindings `ofod_cpp`. Both are checked against the same numbers.
"""

from collections.abc import Iterator
import datetime
import json
import math
import os
import pathlib
import types
from typing import Any


def golden_dir() -> pathlib.Path:
    """Returns the data/golden directory.

    Uses $OFOD_GOLDEN_DIR if set, otherwise searches upwards from this file
    and from the working directory.
    """
    env = os.environ.get("OFOD_GOLDEN_DIR")
    if env:
        return pathlib.Path(env)
    for start in (pathlib.Path(__file__).resolve(), pathlib.Path.cwd()):
        for parent in (start, *start.parents):
            candidate = parent / "data" / "golden"
            if candidate.is_dir():
                return candidate
    raise FileNotFoundError("data/golden not found; set OFOD_GOLDEN_DIR")


def load_cases(chapter: int) -> list[dict[str, Any]]:
    """Returns the golden cases for `chapter`."""
    path = golden_dir() / f"ch{chapter:02d}.json"
    with path.open(encoding="utf-8") as f:
        cases: list[dict[str, Any]] = json.load(f)["cases"]
    return cases


def iter_cases(*chapters: int) -> Iterator[dict[str, Any]]:
    """Yields the cases of each chapter in turn."""
    for chapter in chapters:
        yield from load_cases(chapter)


def _compounding(lib: types.ModuleType, frequency: int) -> Any:
    c = lib.core.Compounding
    return c.continuous() if frequency == 0 else c.periodic(frequency)


def _curve(lib: types.ModuleType, j: dict[str, Any]) -> Any:
    return lib.curves.ZeroCurve(j["times"], j["rates"])


def _bond(lib: types.ModuleType, j: dict[str, Any]) -> Any:
    return lib.instruments.Bond(
        face=j["face"],
        coupon_rate=j["coupon_rate"],
        maturity=j["maturity"],
        frequency=j["frequency"],
    )


def _margin_spec(lib: types.ModuleType, j: dict[str, Any]) -> Any:
    return lib.futures.MarginSpec(
        initial_margin=j["initial_margin"],
        maintenance_margin=j["maintenance_margin"],
        contract_size=j["contract_size"],
        contracts=j["contracts"],
        side=getattr(lib.instruments.Side, j["side"].upper()),
    )


def _date(j: list[int]) -> datetime.date:
    return datetime.date(j[0], j[1], j[2])


def _swap(lib: types.ModuleType, j: dict[str, Any]) -> Any:
    return lib.instruments.InterestRateSwap(
        notional=j["notional"],
        fixed_rate=j["fixed_rate"],
        frequency=j["frequency"],
        payment_times=j["payment_times"],
        next_floating_rate=j["next_floating_rate"],
    )


def _evaluate_m2(lib: types.ModuleType, kind: str, i: dict[str, Any]) -> Any:
    """Chapters 2, 3, 6, 7; returns None when `kind` is not one of them."""
    f = lib.futures
    p = lib.pricers
    r = lib.risk
    long = lib.instruments.Side.LONG
    if kind in ("margin_balances", "margin_calls", "margin_cumulative_gain"):
        days = f.simulate_margin_account(_margin_spec(lib, i), i["prices"])
        if kind == "margin_cumulative_gain":
            return days[-1].cumulative_gain
        if kind == "margin_calls":
            return [d.margin_call for d in days]
        return [d.balance for d in days]
    if kind == "min_variance_hedge_ratio":
        return r.minimum_variance_hedge_ratio(
            i["correlation"], i["spot_vol"], i["futures_vol"]
        )
    if kind == "optimal_contracts":
        return r.optimal_contracts(
            i["hedge_ratio"], i["exposure_units"], i["contract_units"]
        )
    if kind == "beta_hedge_contracts":
        return r.beta_hedge_contracts(
            i["beta"], i["portfolio_value"], i["futures_value"]
        )
    if kind == "change_beta_contracts":
        return r.change_beta_contracts(
            i["beta"],
            i["target_beta"],
            i["portfolio_value"],
            i["futures_value"],
        )
    if kind == "parse_thirty_seconds":
        return f.parse_thirty_seconds(i["quote"])
    if kind == "treasury_cash_price":
        accrued = f.accrued_interest(
            _date(i["last_coupon"]),
            _date(i["next_coupon"]),
            _date(i["settlement"]),
            i["coupon"],
        )
        return f.cash_price(f.parse_thirty_seconds(i["quoted"]), accrued)
    if kind == "conversion_factor":
        return f.conversion_factor(i["coupon_rate"], i["years"])
    if kind == "cheapest_to_deliver_costs":
        return [
            f.find_cheapest_to_deliver(
                i["futures"], [f.DeliverableBond(b[0], b[1])]
            ).cost
            for b in i["bonds"]
        ]
    if kind == "treasury_futures_quote":
        rate = i["rate"]
        cash = f.cash_price(i["quoted_bond"], i["accrued_now"])
        coupons_pv = i["coupon"] * math.exp(-rate * i["coupon_time"])
        return f.treasury_futures_quote(
            cash,
            coupons_pv,
            rate,
            i["t"],
            i["accrued_at_delivery"],
            i["conversion_factor"],
        )
    if kind == "convexity_adjustment":
        return f.convexity_adjustment(i["sigma"], i["t1"], i["t2"])
    if kind == "duration_hedge_contracts":
        return f.duration_hedge_contracts(
            i["portfolio_value"],
            i["portfolio_duration"],
            i["futures_value"],
            i["futures_duration"],
        )
    if kind == "swap_fixed_leg":
        return p.fixed_leg_bond_value(
            _swap(lib, i["swap"]), _curve(lib, i["curve"])
        )
    if kind == "swap_floating_leg":
        return p.floating_leg_bond_value(
            _swap(lib, i["swap"]), _curve(lib, i["curve"])
        )
    if kind == "swap_value":
        return p.swap_value(
            _swap(lib, i["swap"]), _curve(lib, i["curve"]), long
        )
    if kind == "swap_value_as_fras":
        return p.swap_value_as_fras(
            _swap(lib, i["swap"]), _curve(lib, i["curve"]), long
        )
    if kind == "currency_swap_value":
        j = i["swap"]
        swap = lib.instruments.CurrencySwap(
            domestic_notional=j["domestic_notional"],
            domestic_rate=j["domestic_rate"],
            foreign_notional=j["foreign_notional"],
            foreign_rate=j["foreign_rate"],
            frequency=j["frequency"],
            payment_times=j["payment_times"],
        )
        flat = lib.curves.ZeroCurve.flat
        return p.currency_swap_value(
            swap,
            flat(i["domestic_rate"]),
            flat(i["foreign_rate"]),
            i["spot"],
            long,
        )
    if kind == "comparative_advantage":
        return p.comparative_advantage_gain(
            i["fixed_a"],
            i["fixed_b"],
            i["floating_spread_a"],
            i["floating_spread_b"],
        )
    return None


def _option(lib: types.ModuleType, j: dict[str, Any]) -> Any:
    ins = lib.instruments
    return ins.VanillaOption(
        option_type=getattr(ins.OptionType, j["type"].upper()),
        exercise=getattr(ins.Exercise, j["exercise"].upper()),
        strike=j["strike"],
        maturity=j["maturity"],
    )


_SCHEMES = {
    "crr": "COX_ROSS_RUBINSTEIN",
    "jr": "JARROW_RUDD",
    "tian": "TIAN",
}


def _evaluate_m3(lib: types.ModuleType, kind: str, i: dict[str, Any]) -> Any:
    """Chapters 10-13; returns None when `kind` is not one of them."""
    ins = lib.instruments
    p = lib.pricers
    if kind == "naked_option_margin":
        t = getattr(ins.OptionType, i["option_type"].upper())
        margin = ins.naked_option_margin(
            t, i["premium"], i["spot"], i["strike"]
        )
        return i["units"] * margin
    if kind == "adjust_for_split":
        terms = ins.adjust_for_split(
            ins.ContractTerms(i["strike"], i["shares"]), i["n"], i["m"]
        )
        return [terms.strike, terms.shares]
    if kind == "adjust_for_stock_dividend":
        terms = ins.adjust_for_stock_dividend(
            ins.ContractTerms(i["strike"], i["shares"]), i["fraction"]
        )
        return [terms.strike, terms.shares]
    if kind == "european_call_lower_bound":
        return p.european_call_lower_bound(
            i["spot"], i["strike"], i["rate"], i["t"]
        )
    if kind == "european_put_lower_bound":
        return p.european_put_lower_bound(
            i["spot"], i["strike"], i["rate"], i["t"]
        )
    if kind == "put_from_parity":
        return p.put_from_parity(
            i["call"], i["spot"], i["strike"], i["rate"], i["t"]
        )
    if kind == "american_call_minus_put":
        r = p.american_call_minus_put(i["spot"], i["strike"], i["rate"], i["t"])
        return [r.lower, r.upper]
    if kind in ("strategy_profit", "strategy_cost"):
        portfolio = getattr(lib.strategies, i["strategy"])(*i["args"])
        if kind == "strategy_cost":
            return portfolio.cost()
        return [portfolio.profit_at(s) for s in i["spots"]]
    if kind == "principal_protected_budget":
        return lib.strategies.principal_protected_budget(
            i["principal"], i["rate"], i["t"]
        )
    if kind in ("tree_price", "tree_delta"):
        option = _option(lib, i["option"])
        growth = math.exp(i["rate"] * option.maturity / i["steps"])
        step = p.TreeStep(
            i["up"],
            i["down"],
            p.risk_neutral_probability(i["up"], i["down"], growth),
        )
        result = p.price_tree(option, i["spot"], i["rate"], i["steps"], step)
        return result.price if kind == "tree_price" else result.delta
    if kind == "binomial_price":
        m = i["market"]
        market = p.TreeMarket(
            spot=m["spot"],
            rate=m["rate"],
            yield_=m["yield"],
            volatility=m["volatility"],
        )
        scheme = getattr(p.TreeScheme, _SCHEMES[i["scheme"]])
        return p.price_binomial(
            _option(lib, i["option"]), market, i["steps"], scheme
        ).price
    return None


def _otype(lib: types.ModuleType, name: str) -> Any:
    return getattr(lib.instruments.OptionType, name.upper())


def _flows(lib: types.ModuleType, j: list[list[float]]) -> list[Any]:
    return [lib.instruments.CashFlow(f[0], f[1]) for f in j]


def _evaluate_m4(lib: types.ModuleType, kind: str, i: dict[str, Any]) -> Any:
    """Chapters 14-18; returns None when `kind` is not one of them."""
    p = lib.pricers
    if kind.startswith("gbm_"):
        gbm = lib.models.GeometricBrownianMotion(i["drift"], i["volatility"])
        if kind == "gbm_euler_path":
            return gbm.euler_path(i["s0"], i["dt"], i["shocks"])
        if kind == "gbm_confidence_interval":
            ci = gbm.confidence_interval(i["s0"], i["t"], i["z"])
            return [ci.lower, ci.upper]
        if kind == "gbm_expected_value":
            return gbm.expected_value(i["s0"], i["t"])
        if kind == "gbm_variance":
            return gbm.variance(i["s0"], i["t"])
    if kind == "correlated_normals":
        lower = lib.random.cholesky(i["correlation"])
        return lib.random.correlate(lower, i["z"])
    if kind == "historical_volatility":
        return lib.stats.historical_volatility(
            i["prices"], i["periods_per_year"]
        )
    if kind == "bsm_price":
        return p.bsm_price(
            _otype(lib, i["type"]),
            i["spot"],
            i["strike"],
            i["rate"],
            i["yield"],
            i["volatility"],
            i["t"],
        )
    if kind == "implied_volatility":
        return p.implied_volatility(
            _otype(lib, i["type"]),
            i["price"],
            i["spot"],
            i["strike"],
            i["rate"],
            i["yield"],
            i["t"],
        )
    if kind == "bsm_price_with_dividends":
        return p.bsm_price_with_dividends(
            _otype(lib, i["type"]),
            i["spot"],
            i["strike"],
            i["rate"],
            i["volatility"],
            i["t"],
            _flows(lib, i["dividends"]),
        )
    if kind == "black_american_call":
        return p.black_american_call_approximation(
            i["spot"],
            i["strike"],
            i["rate"],
            i["volatility"],
            i["t"],
            _flows(lib, i["dividends"]),
        )
    if kind == "warrant_price":
        return p.warrant_price(
            i["spot"],
            i["strike"],
            i["rate"],
            i["volatility"],
            i["t"],
            i["shares"],
            i["warrants"],
        )
    if kind == "garman_kohlhagen_price":
        return p.garman_kohlhagen_price(
            _otype(lib, i["type"]),
            i["spot"],
            i["strike"],
            i["rate"],
            i["foreign_rate"],
            i["volatility"],
            i["t"],
        )
    if kind == "black76_price":
        return p.black76_price(
            _otype(lib, i["type"]),
            i["futures"],
            i["strike"],
            i["rate"],
            i["volatility"],
            i["t"],
        )
    if kind == "futures_put_from_parity":
        return p.futures_put_from_parity(
            i["call"], i["futures"], i["strike"], i["rate"], i["t"]
        )
    return None


def _market(lib: types.ModuleType, m: dict[str, Any]) -> Any:
    return lib.pricers.TreeMarket(
        spot=m["spot"],
        rate=m["rate"],
        yield_=m["yield"],
        volatility=m["volatility"],
    )


def _evaluate_m5(lib: types.ModuleType, kind: str, i: dict[str, Any]) -> Any:
    """Chapter 21; returns None when `kind` is not one of its kinds."""
    p = lib.pricers
    if kind == "control_variate_american":
        return p.control_variate_american_price(
            _option(lib, i["option"]), _market(lib, i["market"]), i["steps"]
        )
    if kind == "binomial_with_dividends":
        return p.price_binomial_with_dividends(
            _option(lib, i["option"]),
            _market(lib, i["market"]),
            i["steps"],
            _flows(lib, i["dividends"]),
        ).price
    if kind == "trinomial_price":
        return p.price_trinomial(
            _option(lib, i["option"]), _market(lib, i["market"]), i["steps"]
        ).price
    if kind == "finite_difference":
        g = i["grid"]
        grid = p.FdGrid(
            s_min=g["s_min"],
            s_max=g["s_max"],
            price_steps=g["price_steps"],
            time_steps=g["time_steps"],
            log_space=g["log_space"],
        )
        return p.price_finite_difference(
            _option(lib, i["option"]),
            _market(lib, i["market"]),
            grid,
            getattr(p.FdScheme, i["scheme"].upper()),
            getattr(p.AmericanSolver, i["american"].upper()),
        )
    if kind == "mc_european":
        options = p.McOptions(
            paths=i["paths"],
            seed=i["seed"],
            method=getattr(p.VarianceReduction, i["method"].upper()),
        )
        return p.monte_carlo_european(
            _option(lib, i["option"]), _market(lib, i["market"]), options
        ).price
    if kind == "sobol_points":
        seq = lib.random.Sobol(i["dimensions"])
        return [x for _ in range(i["count"]) for x in seq.next()]
    if kind == "norm_inv":
        return lib.math.norm_inv(i["p"])
    return None


def _inputs(lib: types.ModuleType, i: dict[str, Any]) -> Any:
    return lib.greeks.OptionInputs(
        spot=i["spot"],
        rate=i["rate"],
        yield_=i["yield"],
        volatility=i["volatility"],
        maturity=i["t"],
    )


def _exposure(lib: types.ModuleType, j: list[float]) -> Any:
    return lib.greeks.Exposure(delta=j[0], gamma=j[1], vega=j[2])


def _evaluate_m6(lib: types.ModuleType, kind: str, i: dict[str, Any]) -> Any:
    """Chapters 16, 19 and 20; returns None for other kinds."""
    p, g = lib.pricers, lib.greeks
    if kind in ("bsm_greek", "bump_greek"):
        otype, strike = _otype(lib, i["type"]), i["strike"]
        inputs = _inputs(lib, i)
        if kind == "bsm_greek":
            result = g.bsm_greeks(otype, strike, inputs)
        else:

            def pricer(x: Any) -> float:
                return float(
                    p.bsm_price(
                        otype,
                        x.spot,
                        strike,
                        x.rate,
                        x.yield_,
                        x.volatility,
                        x.maturity,
                    )
                )

            result = g.bump_greeks(pricer, inputs)
        return getattr(result, i["greek"])
    if kind in ("gamma_neutral_hedge", "gamma_vega_neutral_hedge"):
        portfolio = _exposure(lib, i["portfolio"])
        if kind == "gamma_neutral_hedge":
            h = g.gamma_neutral_hedge(portfolio, _exposure(lib, i["option1"]))
        else:
            h = g.gamma_vega_neutral_hedge(
                portfolio,
                _exposure(lib, i["option1"]),
                _exposure(lib, i["option2"]),
            )
        return [h.option1, h.option2, h.underlying]
    if kind == "delta_hedge_ratio":
        experiment = g.HedgeExperiment(
            spot=i["spot"],
            strike=i["strike"],
            rate=i["rate"],
            drift=i["drift"],
            volatility=i["volatility"],
            maturity=i["maturity"],
        )
        return g.simulate_delta_hedge(
            experiment, i["rebalances"], i["paths"], i["seed"]
        ).ratio
    if kind == "vol_surface":
        j = i["surface"]
        surface = lib.curves.VolSurface(
            j["maturities"],
            j["strikes"],
            j["volatilities"],
            getattr(lib.curves.VolInterpolation, i["interpolation"].upper()),
        )
        return surface.volatility(i["strike"], i["t"])
    if kind == "implied_density_bsm":
        k, h = i["strike"], i["spacing"]

        def call(strike: float) -> float:
            return float(
                p.bsm_price(
                    _otype(lib, "call"),
                    i["spot"],
                    strike,
                    i["rate"],
                    i["yield"],
                    i["volatility"],
                    i["t"],
                )
            )

        return p.implied_density(
            call(k - h), call(k), call(k + h), h, i["rate"], i["t"]
        )
    if kind == "strike_from_delta":
        return p.strike_from_delta(
            _otype(lib, i["type"]),
            i["delta"],
            i["spot"],
            i["rate"],
            i["yield"],
            i["volatility"],
            i["t"],
        )
    if kind == "minimum_variance_delta":
        return p.minimum_variance_delta(
            i["delta"], i["vega"], i["spot"], i["t"], i["a"], i["b"], i["c"]
        )
    if kind == "binary_jump_implied_vol":
        otype = _otype(lib, i["type"])
        price = p.binary_jump_price(
            otype, i["strike"], i["rate"], i["t"], i["up"], i["down"], i["p_up"]
        )
        return p.implied_volatility(
            otype, price, i["spot"], i["strike"], i["rate"], 0.0, i["t"]
        )
    if kind == "eso_tree":
        t = i["terms"]
        terms = p.EsoTerms(
            strike=t["strike"],
            maturity=t["maturity"],
            vesting=t["vesting"],
            exercise_multiple=t["exercise_multiple"],
            exit_rate=t["exit_rate"],
        )
        return p.price_employee_stock_option(
            terms, _market(lib, i["market"]), i["steps"]
        )
    if kind == "expected_life_value":
        return p.expected_life_value(
            i["spot"],
            i["strike"],
            i["rate"],
            i["yield"],
            i["volatility"],
            i["expected_life"],
        )
    return None


def _garch(lib: types.ModuleType, i: dict[str, Any]) -> Any:
    return lib.stats.Garch11(i["omega"], i["alpha"], i["beta"])


def _evaluate_m7(lib: types.ModuleType, kind: str, i: dict[str, Any]) -> Any:
    """Chapters 22 and 23; returns None for other kinds."""
    r, st = lib.risk, lib.stats
    if kind == "normal_var":
        return r.scale_to_horizon(
            r.normal_var(i["sd"], i["confidence"]), i["days"]
        )
    if kind == "normal_es":
        return r.normal_expected_shortfall(i["sd"], i["confidence"])
    if kind in ("portfolio_std_dev", "portfolio_var"):
        sd = r.portfolio_std_dev(i["exposures"], i["vols"], i["correlation"])
        if kind == "portfolio_std_dev":
            return sd
        return r.scale_to_horizon(r.normal_var(sd, i["confidence"]), i["days"])
    if kind == "historical_var":
        return r.historical_var(i["losses"], i["confidence"])
    if kind == "historical_es":
        return r.historical_expected_shortfall(i["losses"], i["confidence"])
    if kind == "volatility_adjusted_changes":
        return r.volatility_adjusted_changes(
            i["changes"], i["vols"], i["current_vol"]
        )
    if kind == "cornish_fisher_var":
        moments = r.Moments(i["mean"], i["std_dev"], i["skewness"])
        return r.cornish_fisher_var(moments, i["confidence"])
    if kind == "delta_gamma_var":
        moments = r.delta_gamma_moments(i["delta"], i["gamma"], i["sd_change"])
        return r.cornish_fisher_var(moments, i["confidence"])
    if kind == "kupiec":
        return r.kupiec_statistic(i["observations"], i["exceptions"], i["p"])
    if kind == "binomial_tail":
        return r.binomial_tail_probability(i["n"], i["m"], i["p"])
    if kind == "pca_std_dev":
        return r.pca_std_dev(i["exposures"], i["loadings"], i["factor_sds"])
    if kind == "pca_variances":
        return list(r.principal_components(i["observations"]).variances)
    if kind == "pca_loading":
        pca = r.principal_components(i["observations"])
        return list(pca.loadings[i["factor"]])
    if kind == "ewma_vol":
        v = st.ewma_variance_update(i["vol"] ** 2, i["ret"], i["lambda"])
        return math.sqrt(v)
    if kind == "garch_long_run_variance":
        return _garch(lib, i).long_run_variance()
    if kind == "garch_update_vol":
        return math.sqrt(_garch(lib, i).update(i["vol"] ** 2, i["ret"]))
    if kind == "garch_expected_variance":
        return _garch(lib, i).expected_variance(i["vol"] ** 2, i["days"])
    if kind == "garch_term_vol":
        return _garch(lib, i).term_structure_volatility(
            i["vol"] ** 2, i["days"]
        )
    if kind == "ewma_correlation":
        vx, vy, lam = i["vol_x"], i["vol_y"], i["lambda"]
        cov = st.ewma_covariance_update(
            i["rho"] * vx * vy, i["ret_x"], i["ret_y"], lam
        )
        var_x = st.ewma_variance_update(vx * vx, i["ret_x"], lam)
        var_y = st.ewma_variance_update(vy * vy, i["ret_y"], lam)
        return cov / math.sqrt(var_x * var_y)
    if kind == "is_psd":
        return 1.0 if st.is_positive_semidefinite(i["matrix"]) else 0.0
    if kind == "min_eigenvalue":
        return lib.math.symmetric_eigen(i["matrix"]).values[-1]
    if kind == "garch_fit":
        fit = st.fit_garch11(i["returns"])
        return [fit.omega * 1e6, fit.alpha, fit.beta]
    if kind == "garch_fit_log_likelihood":
        return st.fit_garch11(i["returns"]).log_likelihood(i["returns"])
    if kind == "ewma_log_likelihood":
        return st.ewma_log_likelihood(i["returns"], i["lambda"])
    if kind in ("ljung_box_squared", "ljung_box_garch"):
        u = i["returns"]
        v = [1.0] * len(u)
        if kind == "ljung_box_garch":
            v = list(st.fit_garch11(u).variances(u))
        z = [u[k] * u[k] / v[k] for k in range(1, len(u))]
        return st.ljung_box(z, i["lags"])
    return None


def _cds_terms(lib: types.ModuleType, i: dict[str, Any]) -> Any:
    return lib.credit.CdsTerms(i["maturity"], i["frequency"], i["recovery"])


def _evaluate_m8(lib: types.ModuleType, kind: str, i: dict[str, Any]) -> Any:
    """Chapters 9, 24 and 25; returns None for other kinds."""
    c, cv = lib.credit, lib.curves
    if kind == "hazard_from_spread":
        return cv.hazard_from_spread(i["spread"], i["recovery"])
    if kind == "average_hazard":
        return cv.average_hazard(i["cumulative_default"], i["t"])
    if kind == "survival_probability":
        curve = cv.CreditCurve(i["times"], i["hazards"])
        return curve.survival_probability(i["t"])
    if kind == "merton":
        m = c.merton_solve(
            i["equity"], i["equity_vol"], i["debt"], i["rate"], i["t"]
        )
        return getattr(m, i["output"])
    if kind == "bivariate_normal":
        return lib.math.bivariate_norm_cdf(i["a"], i["b"], i["rho"])
    if kind == "joint_default":
        return c.joint_default_probability(i["q1"], i["q2"], i["rho"])
    if kind == "wcdr":
        return c.wcdr(i["pd"], i["rho"], i["confidence"])
    if kind == "vasicek_credit_loss":
        return c.vasicek_credit_loss(
            i["exposure"], i["pd"], i["lgd"], i["rho"], i["confidence"]
        )
    if kind in ("cds_leg", "cds_par_spread", "cds_seller_value", "cds_upfront"):
        terms = _cds_terms(lib, i)
        credit = cv.CreditCurve.flat(i["hazard"])
        discount = cv.ZeroCurve.flat(i["rate"])
        if kind == "cds_leg":
            legs = c.cds_leg_values(terms, credit, discount)
            return getattr(legs, i["output"])
        if kind == "cds_par_spread":
            return c.cds_par_spread(terms, credit, discount)
        if kind == "cds_seller_value":
            return -c.cds_buyer_value(terms, i["spread"], credit, discount)
        return c.cds_upfront(terms, i["coupon"], credit, discount)
    if kind in ("cds_bootstrap_reprice", "cds_bootstrap_hazards"):
        discount = cv.ZeroCurve.flat(i["rate"])
        curve = c.bootstrap_credit_curve(
            i["maturities"],
            i["spreads"],
            i["frequency"],
            i["recovery"],
            discount,
        )
        if kind == "cds_bootstrap_hazards":
            return list(curve.hazards)
        return [
            c.cds_par_spread(
                c.CdsTerms(m, i["frequency"], i["recovery"]), curve, discount
            )
            for m in i["maturities"]
        ]
    if kind == "cds_option":
        return c.cds_option_price(
            i["payer"],
            i["forward"],
            i["strike"],
            i["vol"],
            i["expiry"],
            i["annuity"],
        )
    if kind in ("tranche_spread_bp", "implied_correlation"):
        pj, tj = i["pool"], i["terms"]
        pool = c.CdoPool(pj["names"], pj["hazard"], pj["recovery"])
        terms = c.CdoTerms(
            tj["maturity"], tj["frequency"], tj["quadrature_points"]
        )
        tranche = c.Tranche(*i["tranche"])
        discount = cv.ZeroCurve.flat(i["rate"])
        spread = c.tranche_spread(
            tranche, pool, i["correlation"], terms, discount
        )
        if kind == "tranche_spread_bp":
            return 1e4 * spread
        return c.implied_tranche_correlation(
            tranche, pool, spread, terms, discount
        )
    if kind == "nth_to_default_bp":
        terms = c.BasketTerms(
            i["names"],
            i["nth"],
            i["hazard"],
            i["recovery"],
            i["correlation"],
            i["maturity"],
            i["frequency"],
        )
        r = c.nth_to_default_spread(terms, i["rate"], i["paths"], i["seed"])
        return 1e4 * r.spread
    if kind in ("cva", "forward_cva"):
        if kind == "cva":
            times, exposures = i["times"], i["exposures"]
        else:
            call = lib.instruments.OptionType.CALL
            ee = lib.pricers.bsm_price(
                call,
                i["spot"],
                i["strike"],
                i["rate"],
                0.0,
                i["volatility"],
                i["t"],
            )
            times, exposures = [i["t"]], [ee]
        return c.cva(
            times, exposures, cv.CreditCurve.flat(i["hazard"]), i["recovery"]
        )
    if kind == "fva":
        return c.funding_value_adjustment(
            i["times"], i["exposures"], i["spread"]
        )
    if kind == "forward_ee_mc":
        p = c.simulate_forward_exposure(
            i["spot"],
            i["strike"],
            i["rate"],
            i["volatility"],
            i["t"],
            i["steps"],
            i["paths"],
            i["seed"],
            0.975,
            i.get("threshold", 1e300),
        )
        return p.expected_exposure[-1]
    return None


def evaluate(lib: types.ModuleType, kind: str, inputs: dict[str, Any]) -> Any:
    """Returns what `lib` computes for one golden case.

    Returns:
        A float, or a list of floats for curve outputs.

    Raises:
        KeyError: If `kind` is not known.
    """
    p = lib.pricers
    i = inputs
    if kind == "growth_factor":
        c = _compounding(lib, i["frequency"])
        return i["principal"] * lib.core.growth_factor(i["rate"], i["t"], c)
    if kind == "equivalent_rate":
        return lib.core.equivalent_rate(
            i["rate"],
            _compounding(lib, i["from_frequency"]),
            _compounding(lib, i["to_frequency"]),
        )
    if kind == "bond_price":
        return p.bond_price(_bond(lib, i["bond"]), _curve(lib, i["curve"]))
    if kind == "bond_yield":
        return p.bond_yield(_bond(lib, i["bond"]), i["price"])
    if kind == "par_yield":
        return p.par_yield(
            i["maturity"], i["frequency"], _curve(lib, i["curve"])
        )
    if kind == "bond_price_from_yield":
        return p.bond_price_from_yield(_bond(lib, i["bond"]), i["yield"])
    if kind == "macaulay_duration":
        return p.macaulay_duration(_bond(lib, i["bond"]), i["yield"])
    if kind == "modified_duration":
        return p.modified_duration(
            _bond(lib, i["bond"]),
            i["yield"],
            _compounding(lib, i["frequency"]),
        )
    if kind == "convexity":
        return p.convexity(_bond(lib, i["bond"]), i["yield"])
    if kind == "bootstrap":
        quotes = [
            lib.curves.BondQuote(_bond(lib, q["bond"]), q["price"])
            for q in i["quotes"]
        ]
        return list(lib.curves.bootstrap_from_bonds(quotes).zero_rates)
    if kind == "forward_rates":
        curve = _curve(lib, i["curve"])
        t = curve.times
        return [
            curve.forward_rate(a, b) for a, b in zip(t, t[1:], strict=False)
        ]
    if kind == "forward_price":
        return p.forward_price(i["spot"], i["rate"], i["t"])
    if kind == "forward_price_with_income":
        curve = _curve(lib, i["curve"])
        flows = [
            lib.instruments.CashFlow(f["time"], f["amount"])
            for f in i["income"]
        ]
        return p.forward_price_with_income(
            i["spot"],
            p.present_value(flows, curve),
            curve.zero_rate(i["t"]),
            i["t"],
        )
    if kind == "forward_price_with_yield":
        q = lib.core.equivalent_rate(
            i["yield"],
            _compounding(lib, i["yield_frequency"]),
            lib.core.Compounding.continuous(),
        )
        return p.forward_price_with_yield(i["spot"], i["rate"], q, i["t"])
    if kind == "forward_value":
        f = p.forward_price(i["spot"], i["rate"], i["t"])
        return p.forward_contract_value(
            f, i["delivery_price"], i["rate"], i["t"], lib.instruments.Side.LONG
        )
    for extra in (
        _evaluate_m2,
        _evaluate_m3,
        _evaluate_m4,
        _evaluate_m5,
        _evaluate_m6,
        _evaluate_m7,
        _evaluate_m8,
    ):
        result = extra(lib, kind, i)
        if result is not None:
            return result
    raise KeyError(f"unknown golden case kind: {kind}")


def check_case(lib: types.ModuleType, case: dict[str, Any]) -> None:
    """Asserts that `lib` reproduces one golden case within its tolerance."""
    actual = evaluate(lib, case["kind"], case["inputs"])
    expected = case["expected"]
    tol = case["tol"]
    label = f"{case['id']} ({case['ref']})"
    if isinstance(expected, list):
        assert len(actual) == len(expected), label
        for a, e in zip(actual, expected, strict=True):
            assert abs(a - e) <= tol, f"{label}: {a} vs {e}"
    else:
        assert abs(actual - expected) <= tol, f"{label}: {actual} vs {expected}"
