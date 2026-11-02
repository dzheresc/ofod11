"""Fixed-coupon bullet bond (Hull, Ch 4.6)."""

import dataclasses

from ofod.instruments import cash_flow


@dataclasses.dataclass(frozen=True)
class Bond:
    """A bullet bond.

    Attributes:
        face: Principal repaid at maturity.
        coupon_rate: Annual coupon as a fraction of face.
        maturity: Years from today.
        frequency: Coupons per year.
    """

    face: float = 100.0
    coupon_rate: float = 0.0
    maturity: float = 0.0
    frequency: int = 2


def cash_flows(bond: Bond) -> list[cash_flow.CashFlow]:
    """Returns the bond's remaining cash flows in increasing time order.

    Coupons fall at maturity - k / frequency; a zero-coupon bond pays only
    its face.
    """
    epsilon = 1e-9
    coupon = bond.face * bond.coupon_rate / bond.frequency
    period = 1.0 / bond.frequency
    flows: list[cash_flow.CashFlow] = []
    k = 0
    while (t := bond.maturity - k * period) > epsilon:
        if coupon != 0.0 or k == 0:
            flows.append(cash_flow.CashFlow(t, coupon))
        k += 1
    flows.reverse()
    last = flows[-1]
    flows[-1] = cash_flow.CashFlow(last.time, last.amount + bond.face)
    return flows
