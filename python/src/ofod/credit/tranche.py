"""Tranche losses in a securitization waterfall (Hull, Ch 8)."""

from collections.abc import Iterable
import dataclasses


@dataclasses.dataclass(frozen=True)
class Tranche:
    """A tranche absorbing pool losses between two fractions of principal."""

    attachment: float = 0.0
    detachment: float = 1.0


def tranche_loss(tranche: Tranche, pool_loss: float) -> float:
    """Returns the fraction of the tranche's principal lost."""
    width = tranche.detachment - tranche.attachment
    return min(max((pool_loss - tranche.attachment) / width, 0.0), 1.0)


def waterfall_losses(
    tranches: Iterable[Tranche], pool_loss: float
) -> list[float]:
    """Returns the loss fraction of each tranche for a pool loss."""
    return [tranche_loss(t, pool_loss) for t in tranches]
