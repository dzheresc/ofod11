"""Dated cash flows."""

import dataclasses


@dataclasses.dataclass(frozen=True)
class CashFlow:
    """An amount paid at a time measured in years from today."""

    time: float
    amount: float
