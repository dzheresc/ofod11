"""Unit and property tests for the M3 modules (Ch 10-13)."""

from hypothesis import given
from hypothesis import settings
from hypothesis import strategies as st
import pytest

from ofod import instruments
from ofod import pricers
from ofod import strategies

_CALL = instruments.OptionType.CALL
_PUT = instruments.OptionType.PUT
_AMERICAN = instruments.Exercise.AMERICAN
_EUROPEAN = instruments.Exercise.EUROPEAN


def test_box_spread_is_riskless() -> None:
    box = strategies.box_spread(50.0, 6.0, 1.0, 60.0, 1.5, 6.0)
    assert {box.payoff_at(s) for s in (30.0, 55.0, 80.0)} == {10.0}


def test_replicate_payoff() -> None:
    p = strategies.replicate_payoff(5.0, [0.0, 1.0, 0.0], [40.0, 50.0])
    assert [p.payoff_at(s) for s in (30.0, 45.0, 70.0)] == [5.0, 10.0, 15.0]
    with pytest.raises(ValueError):
        strategies.replicate_payoff(0.0, [0.0, 1.0, 0.0], [50.0, 40.0])
    with pytest.raises(ValueError):
        strategies.replicate_payoff(0.0, [0.0, 1.0], [40.0, 50.0])


def test_early_exercise_rule() -> None:
    assert not pricers.call_early_exercise_may_be_optimal(0.5, 40, 0.09, 0.25)
    assert pricers.call_early_exercise_may_be_optimal(1.0, 40, 0.09, 0.25)


def test_invalid_tree_raises() -> None:
    call = instruments.VanillaOption(strike=21.0, maturity=0.25)
    with pytest.raises(ValueError):
        pricers.price_tree(call, 20.0, 0.12, 1, pricers.TreeStep(1.1, 0.9, 1.2))
    with pytest.raises(ValueError):
        pricers.price_binomial(call, pricers.TreeMarket(20.0, 0.1, 0, 0.2), 0)


@settings(max_examples=60, deadline=None)
@given(
    spot=st.floats(min_value=20.0, max_value=80.0),
    strike=st.floats(min_value=20.0, max_value=80.0),
    rate=st.floats(min_value=0.0, max_value=0.15),
    vol=st.floats(min_value=0.05, max_value=0.8),
    t=st.floats(min_value=0.05, max_value=3.0),
    scheme=st.sampled_from(list(pricers.TreeScheme)),
)
def test_tree_prices_respect_bounds(
    spot: float,
    strike: float,
    rate: float,
    vol: float,
    t: float,
    scheme: pricers.TreeScheme,
) -> None:
    market = pricers.TreeMarket(spot=spot, rate=rate, volatility=vol)
    # JR's p = 1/2 is risk neutral only to O(dt), so allow a small slack.
    exact = scheme is not pricers.TreeScheme.JARROW_RUDD
    eps = 1e-9 if exact else 1e-3 * spot

    def price(kind: instruments.OptionType, ex: instruments.Exercise) -> float:
        option = instruments.VanillaOption(kind, ex, strike, t)
        return pricers.price_binomial(option, market, 40, scheme).price

    c = price(_CALL, _EUROPEAN)
    p = price(_PUT, _EUROPEAN)
    big_c = price(_CALL, _AMERICAN)
    big_p = price(_PUT, _AMERICAN)
    lower_c = pricers.european_call_lower_bound(spot, strike, rate, t)
    assert lower_c - eps <= c <= pricers.call_upper_bound(spot) + eps
    lower_p = pricers.european_put_lower_bound(spot, strike, rate, t)
    upper_p = pricers.european_put_upper_bound(strike, rate, t)
    assert lower_p - eps <= p <= upper_p + eps
    assert big_p <= pricers.american_put_upper_bound(strike) + eps
    assert big_p >= p - eps
    if exact:
        # With an exact risk-neutral p (not JR's p = 1/2): an American call
        # on a non-dividend stock is never exercised early, and put-call
        # parity holds on the tree itself.
        assert big_c == pytest.approx(c, abs=1e-9)
        parity = pricers.put_from_parity(c, spot, strike, rate, t)
        assert parity == pytest.approx(p, abs=1e-8)
        r = pricers.american_call_minus_put(spot, strike, rate, t)
        assert r.lower - eps <= big_c - big_p <= r.upper + eps
