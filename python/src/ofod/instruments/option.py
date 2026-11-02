"""Vanilla options and exchange contract mechanics (Hull, Ch 10)."""

import dataclasses
import enum


class OptionType(enum.Enum):
    """Call or put."""

    CALL = "call"
    PUT = "put"


class Exercise(enum.Enum):
    """Exercise style."""

    EUROPEAN = "european"
    AMERICAN = "american"


@dataclasses.dataclass(frozen=True)
class VanillaOption:
    """A call or put on one unit of the underlying.

    Attributes:
        option_type: Call or put.
        exercise: European or American.
        strike: Strike price.
        maturity: Years to expiry.
    """

    option_type: OptionType = OptionType.CALL
    exercise: Exercise = Exercise.EUROPEAN
    strike: float = 0.0
    maturity: float = 0.0


def intrinsic_value(option: VanillaOption, spot: float) -> float:
    """Returns max(S - K, 0) for a call and max(K - S, 0) for a put."""
    if option.option_type is OptionType.CALL:
        return max(spot - option.strike, 0.0)
    return max(option.strike - spot, 0.0)


@dataclasses.dataclass(frozen=True)
class ContractTerms:
    """Strike and number of shares covered by one exchange contract."""

    strike: float = 0.0
    shares: float = 100.0


def adjust_for_split(terms: ContractTerms, n: int, m: int) -> ContractTerms:
    """Returns the terms after an n-for-m stock split (Ch 10.5)."""
    ratio = n / m
    return ContractTerms(terms.strike / ratio, terms.shares * ratio)


def adjust_for_stock_dividend(
    terms: ContractTerms, fraction: float
) -> ContractTerms:
    """Treats a stock dividend as a (1 + fraction)-for-1 split."""
    return ContractTerms(
        terms.strike / (1.0 + fraction), terms.shares * (1.0 + fraction)
    )


def naked_option_margin(
    option_type: OptionType, premium: float, spot: float, strike: float
) -> float:
    """Returns the CBOE initial margin per unit for a written option.

    The greater of premium + 20% of the spot less the amount out of the
    money, and premium + 10% of the spot (call) or strike (put) (Ch 10.8).
    """
    if option_type is OptionType.CALL:
        out_of_money = max(strike - spot, 0.0)
        return max(premium + 0.2 * spot - out_of_money, premium + 0.1 * spot)
    out_of_money = max(spot - strike, 0.0)
    return max(premium + 0.2 * spot - out_of_money, premium + 0.1 * strike)
