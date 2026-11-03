"""Hull's trinomial tree for a mean-reverting commodity price (Ch 35.4).

x = ln S follows dx = (theta(t) - a x) dt + sigma dz. The tree is built in x
as for Hull-White and each step is shifted so that the expected price
matches the futures price for that maturity.
"""

from collections.abc import Sequence
import math

from ofod.instruments.option import Exercise
from ofod.instruments.option import OptionType
from ofod.pricers.short_rate_tree import Branch
from ofod.pricers.short_rate_tree import hull_branch


class CommodityTree:
    """A trinomial tree for ln S fitted to futures prices."""

    def __init__(
        self,
        spot: float,
        futures: Sequence[float],
        a: float,
        sigma: float,
        dt: float,
    ) -> None:
        """Builds the tree; futures[i] is for maturity (i + 1) dt.

        Raises:
            ValueError: For non-positive prices, dt, a or sigma, or no futures.
        """
        if (
            not spot > 0.0
            or not futures
            or not a > 0.0
            or not sigma > 0.0
            or not dt > 0.0
            or any(not f > 0.0 for f in futures)
        ):
            raise ValueError("invalid commodity tree inputs")
        self._steps = len(futures)
        self._dt = dt
        self._dx = sigma * math.sqrt(3.0 * dt)
        self._jmax = min(math.floor(0.184 / (a * dt)) + 1, self._steps)
        m = -a * dt
        self._branches = [
            hull_branch(j, self._jmax, m)
            for j in range(-self._jmax, self._jmax + 1)
        ]
        self._alphas = [math.log(spot)]
        self._p: list[list[float]] = [[1.0]]
        for i in range(self._steps):
            w = self.width(i)
            w_next = self.width(i + 1)
            p = self._p[-1]
            nxt = [0.0] * (2 * w_next + 1)
            for j in range(-w, w + 1):
                b = self.branch_at(j)
                here = p[j + w]
                nxt[b.middle + 1 + w_next] += here * b.up
                nxt[b.middle + w_next] += here * b.mid
                nxt[b.middle - 1 + w_next] += here * b.down
            total = sum(
                nxt[j + w_next] * math.exp(j * self._dx)
                for j in range(-w_next, w_next + 1)
            )
            self._alphas.append(math.log(futures[i]) - math.log(total))
            self._p.append(nxt)

    @property
    def steps(self) -> int:
        """Number of steps."""
        return self._steps

    @property
    def dt(self) -> float:
        """Step length."""
        return self._dt

    @property
    def dx(self) -> float:
        """Node spacing sigma sqrt(3 dt)."""
        return self._dx

    @property
    def jmax(self) -> int:
        """Highest node index."""
        return self._jmax

    @property
    def alphas(self) -> list[float]:
        """alpha_i with ln S = alpha_i + j dx; alpha_0 = ln spot."""
        return list(self._alphas)

    def width(self, step: int) -> int:
        """Returns min(step, jmax)."""
        return min(step, self._jmax)

    def price(self, step: int, j: int) -> float:
        """Returns the commodity price at node (step, j)."""
        return math.exp(self._alphas[step] + j * self._dx)

    def branch_at(self, j: int) -> Branch:
        """Returns the branching from node j."""
        return self._branches[j + self._jmax]

    def probabilities(self, step: int) -> list[float]:
        """Returns node probabilities at `step`, indexed by j + width."""
        return list(self._p[step])

    def rollback(
        self, nxt: Sequence[float], step: int, rate: float
    ) -> list[float]:
        """Discounted expectation of step + 1 values at `step`."""
        w = self.width(step)
        w_next = self.width(step + 1)
        disc = math.exp(-rate * self._dt)
        out = []
        for j in range(-w, w + 1):
            b = self.branch_at(j)
            out.append(
                disc
                * (
                    b.up * nxt[b.middle + 1 + w_next]
                    + b.mid * nxt[b.middle + w_next]
                    + b.down * nxt[b.middle - 1 + w_next]
                )
            )
        return out


def commodity_tree_option(
    tree: CommodityTree,
    option_type: OptionType,
    exercise: Exercise,
    strike: float,
    expiry_step: int,
    rate: float,
) -> float:
    """European or American option on the spot price.

    Raises:
        ValueError: If expiry_step is outside the tree.
    """
    if expiry_step < 0 or expiry_step > tree.steps:
        raise ValueError("expiry step outside the tree")
    sign = 1.0 if option_type is OptionType.CALL else -1.0

    def payoff(step: int, j: int) -> float:
        return max(sign * (tree.price(step, j) - strike), 0.0)

    w = tree.width(expiry_step)
    values = [payoff(expiry_step, j) for j in range(-w, w + 1)]
    for i in range(expiry_step - 1, -1, -1):
        values = tree.rollback(values, i, rate)
        if exercise is Exercise.AMERICAN:
            wi = tree.width(i)
            values = [
                max(v, payoff(i, j))
                for v, j in zip(values, range(-wi, wi + 1), strict=True)
            ]
    return values[0]
