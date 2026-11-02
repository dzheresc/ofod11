"""Credit: securitization tranches (later chapters add default models)."""

from ofod.credit.tranche import Tranche
from ofod.credit.tranche import tranche_loss
from ofod.credit.tranche import waterfall_losses

__all__ = ["Tranche", "tranche_loss", "waterfall_losses"]
