"""Trinomial trees for one-factor short-rate models (Hull, Ch 32.6-32.8).

The two-stage Hull-White construction for normal rates and the same tree
in ln r for Black-Karasinski, fitted exactly to today's zero curve, with
the bond options and Bermudan swaptions priced on them.
"""

from collections.abc import Sequence
import dataclasses
import enum
import math

import numpy as np

from ofod.curves import zero_curve
from ofod.instruments.option import Exercise
from ofod.instruments.option import OptionType
from ofod.math import roots


class ShortRateModel(enum.Enum):
    """Normal (Hull-White) or lognormal (Black-Karasinski) rates."""

    HULL_WHITE = 0
    BLACK_KARASINSKI = 1


@dataclasses.dataclass(frozen=True)
class Branch:
    """Middle node j at the next step and the three probabilities."""

    middle: int
    up: float
    mid: float
    down: float


@dataclasses.dataclass(frozen=True)
class TreeCashFlow:
    """A cash flow paid at a tree step."""

    step: int
    amount: float


def _make_branch(j: int, jmax: int, m: float) -> Branch:
    jm = j * m
    jm2 = jm * jm
    if j == jmax and jmax > 0:
        return Branch(
            j - 1,
            7.0 / 6.0 + 0.5 * (jm2 + 3.0 * jm),
            -1.0 / 3.0 - jm2 - 2.0 * jm,
            1.0 / 6.0 + 0.5 * (jm2 + jm),
        )
    if j == -jmax and jmax > 0:
        return Branch(
            j + 1,
            1.0 / 6.0 + 0.5 * (jm2 - jm),
            -1.0 / 3.0 - jm2 + 2.0 * jm,
            7.0 / 6.0 + 0.5 * (jm2 - 3.0 * jm),
        )
    return Branch(
        j,
        1.0 / 6.0 + 0.5 * (jm2 + jm),
        2.0 / 3.0 - jm2,
        1.0 / 6.0 + 0.5 * (jm2 - jm),
    )


class ShortRateTree:
    """A trinomial tree fitted to a zero curve."""

    def __init__(
        self,
        curve: zero_curve.ZeroCurve,
        a: float,
        sigma: float,
        dt: float,
        steps: int,
        model: ShortRateModel = ShortRateModel.HULL_WHITE,
    ) -> None:
        """Builds `steps` steps of length `dt`.

        Raises:
            ValueError: For non-positive dt, sigma or steps, a < 0, or a
                Black-Karasinski alpha that cannot be fitted.
        """
        if not dt > 0.0 or not sigma > 0.0 or steps <= 0 or a < 0.0:
            raise ValueError("invalid tree inputs")
        self._steps = steps
        self._dt = dt
        self._model = model
        self._dx = sigma * math.sqrt(3.0 * dt)
        if a > 0.0:
            self._jmax = min(math.floor(0.184 / (a * dt)) + 1, steps)
        else:
            self._jmax = steps
        m = -a * dt
        self._branches = [
            _make_branch(j, self._jmax, m)
            for j in range(-self._jmax, self._jmax + 1)
        ]
        # Hull-White discount factors factor as e^{-alpha dt} e^{-j dx dt}.
        self._node_discount = [
            math.exp(-j * self._dx * dt)
            for j in range(-self._jmax, self._jmax + 1)
        ]
        self._step_discount: list[float] = []
        self._alphas: list[float] = []
        self._q: list[list[float]] = [[1.0]]
        for i in range(steps):
            w = self.width(i)
            q = self._q[-1]
            target = curve.discount_factor((i + 1) * dt)
            js = range(-w, w + 1)
            if model is ShortRateModel.HULL_WHITE:
                total = sum(
                    q[j + w] * self._node_discount[j + self._jmax] for j in js
                )
                alpha = (math.log(total) - math.log(target)) / dt
            else:

                def error(
                    x: float, q: list[float] = q, w: int = w, t: float = target
                ) -> float:
                    return (
                        sum(
                            q[j + w]
                            * math.exp(-math.exp(x + j * self._dx) * dt)
                            for j in range(-w, w + 1)
                        )
                        - t
                    )

                root = roots.brent(error, -30.0, 3.0)
                if root is None:
                    raise ValueError("alpha not found")
                alpha = root
            self._alphas.append(alpha)
            self._step_discount.append(math.exp(-alpha * dt))
            w_next = self.width(i + 1)
            nxt = [0.0] * (2 * w_next + 1)
            for j in js:
                b = self.branch_at(j)
                flow = q[j + w] * self.discount(i, j)
                nxt[b.middle + 1 + w_next] += flow * b.up
                nxt[b.middle + w_next] += flow * b.mid
                nxt[b.middle - 1 + w_next] += flow * b.down
            self._q.append(nxt)

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
        """Fitted displacement at each step."""
        return list(self._alphas)

    def width(self, step: int) -> int:
        """Returns min(step, jmax)."""
        return min(step, self._jmax)

    def rate(self, step: int, j: int) -> float:
        """Returns the dt-period short rate at node (step, j)."""
        x = self._alphas[step] + j * self._dx
        return x if self._model is ShortRateModel.HULL_WHITE else math.exp(x)

    def discount(self, step: int, j: int) -> float:
        """Returns exp(-r dt) at node (step, j)."""
        if self._model is ShortRateModel.HULL_WHITE:
            return (
                self._step_discount[step] * self._node_discount[j + self._jmax]
            )
        return math.exp(-self.rate(step, j) * self._dt)

    def branch_at(self, j: int) -> Branch:
        """Returns the branching from node j."""
        return self._branches[j + self._jmax]

    def arrow_debreu(self, step: int) -> list[float]:
        """Returns Q at `step`, indexed by j + width(step)."""
        return list(self._q[step])

    def rollback(self, nxt: Sequence[float], step: int) -> list[float]:
        """Returns discounted expectations at `step` of values at step + 1."""
        w = self.width(step)
        wn = self.width(step + 1)
        out = []
        for j in range(-w, w + 1):
            b = self.branch_at(j)
            expected = (
                b.up * nxt[b.middle + 1 + wn]
                + b.mid * nxt[b.middle + wn]
                + b.down * nxt[b.middle - 1 + wn]
            )
            out.append(self.discount(step, j) * expected)
        return out

    def zero_bond_values(self, step: int, maturity_step: int) -> list[float]:
        """Returns unit zero-coupon bond prices at every node of `step`."""
        values = [1.0] * (2 * self.width(maturity_step) + 1)
        for s in range(maturity_step - 1, step - 1, -1):
            values = self.rollback(values, s)
        return values


def tree_bond_option(
    tree: ShortRateTree,
    option_type: OptionType,
    exercise: Exercise,
    expiry_step: int,
    bond: Sequence[TreeCashFlow],
    strike: float,
) -> float:
    """Returns a European or American option on a bond on the tree.

    Raises:
        ValueError: For no cash flows or steps outside the tree.
    """
    if not bond:
        raise ValueError("no cash flows")
    if any(c.step < 0 or c.step > tree.steps for c in bond):
        raise ValueError("cash flow outside the tree")
    last = max(c.step for c in bond)
    if expiry_step < 0 or expiry_step > last:
        raise ValueError("bad expiry")
    phi = 1.0 if option_type is OptionType.CALL else -1.0
    value = np.zeros(2 * tree.width(last) + 1)
    option: np.ndarray | None = None
    for s in range(last, -1, -1):
        if s < last:
            value = np.asarray(tree.rollback(value.tolist(), s))
            if option is not None:
                option = np.asarray(tree.rollback(option.tolist(), s))
        if s == expiry_step or (
            s < expiry_step and exercise is Exercise.AMERICAN
        ):
            if option is None:
                option = np.zeros(value.size)
            option = np.maximum(option, phi * (value - strike))
        value = value + sum(c.amount for c in bond if c.step == s)
    assert option is not None
    return float(option[0])


def tree_swaption(
    tree: ShortRateTree,
    option_type: OptionType,
    strike: float,
    period_steps: int,
    end_step: int,
    exercise_steps: Sequence[int],
) -> float:
    """Returns a payer (CALL) or receiver (PUT) co-terminal swaption.

    Raises:
        ValueError: For a bad schedule.
    """
    if period_steps <= 0 or end_step > tree.steps or not exercise_steps:
        raise ValueError("bad schedule")
    for e in exercise_steps:
        if e < 0 or e >= end_step or (end_step - e) % period_steps != 0:
            raise ValueError("exercise step off the payment grid")
    phi = 1.0 if option_type is OptionType.CALL else -1.0
    coupon = strike * period_steps * tree.dt
    fixed = np.zeros(2 * tree.width(end_step) + 1)
    option: np.ndarray | None = None
    for s in range(end_step, -1, -1):
        if s < end_step:
            fixed = np.asarray(tree.rollback(fixed.tolist(), s))
            if option is not None:
                option = np.asarray(tree.rollback(option.tolist(), s))
        if s in exercise_steps:
            if option is None:
                option = np.zeros(fixed.size)
            option = np.maximum(option, phi * (1.0 - fixed))
        if (end_step - s) % period_steps == 0:
            fixed = fixed + coupon + (1.0 if s == end_step else 0.0)
    return 0.0 if option is None else float(option[0])
