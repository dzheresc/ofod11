"""Pure-Python implementation of the Hull OFOD (11th ed.) code companion."""

from ofod import core
from ofod import credit
from ofod import curves
from ofod import futures
from ofod import instruments
from ofod import math
from ofod import pricers
from ofod import risk
from ofod import stats
from ofod import strategies

__version__ = "0.1.0"

__all__ = [
    "core",
    "credit",
    "curves",
    "futures",
    "instruments",
    "math",
    "pricers",
    "risk",
    "stats",
    "strategies",
]
