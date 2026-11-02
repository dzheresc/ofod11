"""Option trading strategies as portfolios (Hull, Ch 12)."""

from ofod.strategies.strategies import bear_call_spread
from ofod.strategies.strategies import bear_put_spread
from ofod.strategies.strategies import box_spread
from ofod.strategies.strategies import bull_call_spread
from ofod.strategies.strategies import bull_put_spread
from ofod.strategies.strategies import butterfly_call_spread
from ofod.strategies.strategies import covered_call
from ofod.strategies.strategies import principal_protected_budget
from ofod.strategies.strategies import protective_put
from ofod.strategies.strategies import replicate_payoff
from ofod.strategies.strategies import straddle
from ofod.strategies.strategies import strangle
from ofod.strategies.strategies import strap
from ofod.strategies.strategies import strip

__all__ = [
    "bear_call_spread",
    "bear_put_spread",
    "box_spread",
    "bull_call_spread",
    "bull_put_spread",
    "butterfly_call_spread",
    "covered_call",
    "principal_protected_budget",
    "protective_put",
    "replicate_payoff",
    "straddle",
    "strangle",
    "strap",
    "strip",
]
