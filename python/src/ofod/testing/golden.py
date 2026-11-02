"""Runs the book's golden cases (data/golden/*.json) against a library.

`lib` is any module that exposes the ofod API: `ofod` itself, or the C++
bindings `ofod_cpp`. Both are checked against the same numbers.
"""

from collections.abc import Iterator
import json
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
