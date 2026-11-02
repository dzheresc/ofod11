"""Short-term rate futures and duration-based hedging (Hull, Ch 6.3-6.4)."""


def futures_rate_from_quote(quote: float) -> float:
    """Returns the futures rate implied by a quote of 100 - rate (in %)."""
    return (100.0 - quote) / 100.0


def three_month_contract_value(quote: float) -> float:
    """Returns 10,000 [100 - 0.25 (100 - Q)] for a three-month contract."""
    return 10000.0 * (100.0 - 0.25 * (100.0 - quote))


def convexity_adjustment(sigma: float, t1: float, t2: float) -> float:
    """Returns sigma^2 t1 t2 / 2 (Ho-Lee, eq. 6.3).

    Subtract it from the futures rate to get the forward rate.
    """
    return 0.5 * sigma * sigma * t1 * t2


def duration_hedge_contracts(
    portfolio_value: float,
    portfolio_duration: float,
    futures_value: float,
    futures_duration: float,
) -> float:
    """Returns N* = P D_P / (V_F D_F) (eq. 6.5)."""
    return (
        portfolio_value
        * portfolio_duration
        / (futures_value * futures_duration)
    )
