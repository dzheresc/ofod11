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
    result = _evaluate_m2(lib, kind, i)
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
