"""Unit tests for the M2 modules (Ch 2, 3, 6, 7, 8)."""

import pytest

from ofod import credit
from ofod import curves
from ofod import futures
from ofod import instruments
from ofod import pricers
from ofod import risk
from ofod import stats


def test_regression_recovers_a_line() -> None:
    x = [1.0, 2.0, 3.0, 4.0]
    y = [3.0, 5.0, 7.0, 9.0]
    fit = stats.linear_regression(x, y)
    assert fit.slope == pytest.approx(2.0)
    assert fit.intercept == pytest.approx(1.0)
    assert fit.r_squared == pytest.approx(1.0)
    assert stats.correlation(x, y) == pytest.approx(1.0)


def test_stats_reject_bad_samples() -> None:
    with pytest.raises(ValueError):
        stats.mean([])
    with pytest.raises(ValueError):
        stats.variance([1.0])
    with pytest.raises(ValueError):
        stats.correlation([1.0, 1.0], [1.0, 2.0])


def test_hedge_ratio_from_changes_is_regression_slope() -> None:
    futures_changes = [0.1, -0.2, 0.3, 0.05]
    spot_changes = [0.8 * f for f in futures_changes]
    assert risk.hedge_ratio_from_changes(
        spot_changes, futures_changes
    ) == pytest.approx(0.8)


def test_short_roll_gain() -> None:
    legs = [risk.RollLeg(68.20, 67.40), risk.RollLeg(67.00, 66.50)]
    assert risk.short_roll_gain(legs) == pytest.approx(1.30)


def test_short_margin_account_gains_when_price_falls() -> None:
    spec = futures.MarginSpec(
        initial_margin=10.0,
        maintenance_margin=5.0,
        contracts=1,
        side=instruments.Side.SHORT,
    )
    days = futures.simulate_margin_account(spec, [100.0, 98.0])
    assert days[-1].daily_gain == 2.0
    assert days[-1].margin_call == 0.0


@pytest.mark.parametrize("quote", ["95", "95-32", "95-1x", "-16"])
def test_parse_thirty_seconds_rejects(quote: str) -> None:
    with pytest.raises(ValueError):
        futures.parse_thirty_seconds(quote)


def test_cheapest_to_deliver_picks_lowest_cost() -> None:
    bonds = [
        futures.DeliverableBond(99.50, 1.0382),
        futures.DeliverableBond(143.50, 1.5188),
        futures.DeliverableBond(119.75, 1.2615),
    ]
    ctd = futures.find_cheapest_to_deliver(93.25, bonds)
    assert ctd.index == 1
    with pytest.raises(ValueError):
        futures.find_cheapest_to_deliver(93.25, [])


def test_six_percent_bond_has_unit_conversion_factor() -> None:
    assert futures.conversion_factor(0.06, 20.5) == pytest.approx(1.0)


def test_swaps_bootstrap_reprices_at_par() -> None:
    quotes = [curves.SwapQuote(m, 0.03 + 0.002 * m, 1) for m in (1, 2, 3)]
    curve = curves.bootstrap_from_swaps(quotes)
    for q in quotes:
        times = [float(k) for k in range(1, int(q.maturity) + 1)]
        assert pricers.par_swap_rate(times, 1, curve) == pytest.approx(
            q.par_rate, abs=1e-10
        )


def test_par_swap_has_zero_value() -> None:
    curve = curves.ZeroCurve.flat(0.05)
    times = [0.5, 1.0, 1.5, 2.0]
    rate = pricers.par_swap_rate(times, 2, curve)
    swap = instruments.InterestRateSwap(
        notional=100.0,
        fixed_rate=rate,
        frequency=2,
        payment_times=times,
        next_floating_rate=2.0 * (curve.discount_factor(0.0) - 1.0)
        + 2.0 * (1.0 / curve.discount_factor(0.5) - 1.0),
    )
    side = instruments.Side.LONG
    assert pricers.swap_value_as_fras(swap, curve, side) == pytest.approx(
        pricers.swap_value(swap, curve, side), abs=1e-9
    )


def test_tranche_waterfall() -> None:
    tranches = [
        credit.Tranche(0.0, 0.05),
        credit.Tranche(0.05, 0.20),
        credit.Tranche(0.20, 1.0),
    ]
    assert credit.waterfall_losses(tranches, 0.12) == pytest.approx(
        [1.0, 7.0 / 15.0, 0.0]
    )
