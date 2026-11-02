"""Forward rate agreement (Hull, Ch 4.9)."""

import dataclasses


@dataclasses.dataclass(frozen=True)
class Fra:
    """Exchanges a fixed rate for the floating rate over [start, end].

    Both rates use simple compounding over the accrual period.
    """

    notional: float = 0.0
    fixed_rate: float = 0.0
    start: float = 0.0
    end: float = 0.0
