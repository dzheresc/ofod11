"""Chapter 35: energy and commodity derivatives.

Run with `python chapters/ch35_commodity_derivatives/py/commodities.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math
from typing import Any


def _sample_temperatures(days: int) -> list[float]:
    return [
        52.0
        + 22.0 * math.cos(2.0 * math.pi * (d - 200) / 365.0)
        + 4.0 * math.sin(0.9 * d)
        + 3.0 * math.cos(2.3 * d + 1.0)
        for d in range(days)
    ]


def _seasonal(lib: Any) -> None:
    t = [(k + 0.5) / 12.0 for k in range(24)]
    f = [
        3.0
        * math.exp(0.03 * v + 0.18 * math.cos(2.0 * math.pi * (v - 1.0 / 24.0)))
        for v in t
    ]
    curve = lib.models.fit_seasonal_curve(t, f)
    print(
        "Seasonal gas curve from 24 monthly futures: level "
        f"{math.exp(curve.level):.4f}, growth {curve.growth:.4f}"
    )
    multipliers = "".join(
        f" {curve.multiplier((m + 0.5) / 12.0):.3f}" for m in range(12)
    )
    print(f"  monthly multipliers:{multipliers}")


def _tree(lib: Any) -> None:
    p = lib.pricers
    otype = lib.instruments.OptionType
    ex = lib.instruments.Exercise
    tree = p.CommodityTree(20.0, [22.0, 23.0, 24.0], 0.1, 0.2, 1.0)
    print(
        "Commodity tree, S = 20, futures 22, 23, 24, a = 0.1, sigma = 0.2: "
        f"dx = {tree.dx:.5f}, jmax = {tree.jmax}"
    )
    print("  alphas:" + "".join(f" {a:.5f}" for a in tree.alphas))
    w = tree.width(3)
    print(
        "  prices at year 3:"
        + "".join(f" {tree.price(3, j):.4f}" for j in range(-w, w + 1))
    )
    euro = p.commodity_tree_option(tree, otype.PUT, ex.EUROPEAN, 22.0, 3, 0.05)
    amer = p.commodity_tree_option(tree, otype.PUT, ex.AMERICAN, 22.0, 3, 0.05)
    print(
        f"  3-year put, K = 22, r = 5%: European {euro:.4f}, American "
        f"{amer:.4f}"
    )


def _spikes(lib: Any) -> None:
    md = lib.models
    model = md.SpikeModel(math.log(40.0), 8.0, 0.8, 6.0, 0.8, 0.3)
    calm = md.SpikeModel(math.log(40.0), 8.0, 0.8)
    print(
        "Electricity, $40 level, a = 8, 6 spikes a year: E[S(3 months)] "
        f"{md.spike_expected_price(model, 0.0, 0.25):.4f} (no spikes "
        f"{md.spike_expected_price(calm, 0.0, 0.25):.4f})"
    )
    mc = md.spike_monte_carlo(model, 0.0, 0.25, 50, 20_000, 1)
    path = md.simulate_spike_path(model, 0.0, 1.0 / 365, 365, 2)
    print(
        f"  Monte Carlo {mc.mean:.4f} (se {mc.std_error:.4f}); one simulated "
        f"year peaks at {max(path):.2f}"
    )


def _weather(lib: Any) -> None:
    p = lib.pricers
    otype = lib.instruments.OptionType
    week = [38.0, 45.0, 52.0, 61.0, 66.0, 70.0, 59.0]
    print(
        "Degree days for a week: HDD "
        f"{p.degree_days(p.DegreeDayKind.HEATING, week):.1f}, CDD "
        f"{p.degree_days(p.DegreeDayKind.COOLING, week):.1f}"
    )
    call = p.DegreeDayOption(otype.CALL, 700.0, 10_000.0, 2_000_000.0)
    seasons = [650.0, 720.0, 805.0, 690.0, 760.0]
    seasons += [880.0, 610.0, 702.0, 745.0, 698.0]
    burn = p.burn_analysis(call, seasons, 0.05, 0.5)
    print(
        "HDD call, K = 700, $10,000 a degree day, cap $2m: burn analysis over "
        f"10 seasons {burn.value:.0f} (se {burn.std_error:.0f})"
    )
    model = p.fit_temperature_model(_sample_temperatures(365 * 6))
    print(
        "Temperature model from 6 years of daily data: mean "
        f"{model.mean:.2f}, amplitude {model.amplitude:.2f}, peak day "
        f"{model.phase:.1f}, phi {model.phi:.3f}, sigma {model.sigma:.3f}"
    )
    winter = p.SeasonSpec(p.DegreeDayKind.HEATING, 365 * 6 + 334, 90)
    winter_call = p.DegreeDayOption(otype.CALL, 2_400.0, 5_000.0)
    sim = p.simulate_degree_day_option(
        model, winter, winter_call, 0.05, 0.5, 20_000, 3
    )
    print(
        f"  winter HDD simulated: mean {sim.mean_degree_days:.1f}, sd "
        f"{sim.std_degree_days:.1f}; call at 2400, $5,000 a degree day: "
        f"{sim.option.value:.0f} (se {sim.option.std_error:.0f})"
    )


def _insurance(lib: Any) -> None:
    p = lib.pricers
    hurricanes = p.LossModel(0.3, math.log(300.0), 0.8)
    bond = p.CatBondTerms(100.0, 0.08, 3.0, 500.0, 1_000.0, 2)
    cat = p.price_cat_bond(bond, hurricanes, 0.03, 100_000, 4)
    print(
        "Cat bond, 8% coupon, 3 years, principal lost between $500m and $1bn "
        "of losses:"
    )
    print(
        f"  price {cat.price:.4f} (se {cat.std_error:.4f}), P(loss) "
        f"{cat.loss_probability:.4f}, expected loss {cat.expected_loss:.4f}, "
        f"spread {1e4 * cat.spread:.0f} bp"
    )
    layer = p.excess_of_loss_layer(
        hurricanes, 500.0, 500.0, 1.0, 0.03, 100_000, 5
    )
    print(
        "  one-year excess-of-loss layer, $500m xs $500m: expected cost "
        f"{layer.value:.4f} (se {layer.std_error:.4f})"
    )


def _producer_hedge() -> None:
    print(
        "Producer's revenue ($m) for 1m barrels: spot, unhedged, forward at "
        "70, 60-80 collar"
    )
    for spot in (50.0, 60.0, 70.0, 80.0, 90.0):
        collar = min(max(spot, 60.0), 80.0)
        print(f"  {spot:5.1f}  {spot:6.1f}  {70.0:6.1f}  {collar:6.1f}")


def main() -> None:
    """Runs the chapter's examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    _seasonal(lib)
    _tree(lib)
    _spikes(lib)
    _weather(lib)
    _insurance(lib)
    _producer_hedge()


if __name__ == "__main__":
    main()
