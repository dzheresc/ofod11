"""Chapter 37: derivatives mishaps and what we can learn from them.

Run with `python chapters/ch37_derivatives_mishaps/py/mishaps.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
"""

import argparse
import importlib
import math
from typing import Any

_CARRY = 0.1
_DELIVERIES = 60
_FIXED_PRICE = 22.0
_RATE = 0.05


def _spot(month: int) -> float:
    return 15.0 if month >= 12 else 20.0 - 5.0 * month / 12.0


def _commitment_value(month: int) -> float:
    return sum(
        (_FIXED_PRICE - (_spot(month) + _CARRY * k)) * math.exp(-_RATE * k / 12)
        for k in range(1, _DELIVERIES - month + 1)
    )


def _stack_and_roll() -> None:
    print(
        "Stack-and-roll: deliver 1 unit a month for 5 years at 22, hedged "
        "with a rolled stack of 1-month futures"
    )
    print("  month   spot   futures cash   commitments   net")
    start = _commitment_value(0)
    cash = 0.0
    for month in range(13):
        if month > 0:
            stack = _DELIVERIES - (month - 1)
            cash = cash * math.exp(_RATE / 12.0) + stack * (
                _spot(month) - (_spot(month - 1) + _CARRY)
            )
        if month % 3 == 0:
            gain = _commitment_value(month) - start
            print(
                f"  {month:5d}  {_spot(month):5.2f}  {cash:13.2f}  "
                f"{gain:12.2f}  {cash + gain:6.2f}"
            )


def _model_risk(lib: Any) -> None:
    s0, r, sold_vol, true_vol = 100.0, 0.05, 0.15, 0.25
    steps, paths = 52, 2000
    dt = 1.0 / steps
    call = lib.instruments.OptionType.CALL
    premium = lib.pricers.bsm_price(call, s0, s0, r, 0.0, sold_vol, 1.0)
    fair = lib.pricers.bsm_price(call, s0, s0, r, 0.0, true_vol, 1.0)

    def delta(s: float, tau: float) -> float:
        d1 = (math.log(s / s0) + (r + 0.5 * sold_vol**2) * tau) / (
            sold_vol * math.sqrt(tau)
        )
        return float(lib.math.norm_cdf(d1))

    rng = lib.random.NormalGenerator(37)
    total = 0.0
    total_sq = 0.0
    for _ in range(paths):
        s = s0
        held = delta(s, 1.0)
        bank = premium - held * s
        for i in range(1, steps + 1):
            s *= math.exp(
                (r - 0.5 * true_vol**2) * dt
                + true_vol * math.sqrt(dt) * rng.next()
            )
            bank *= math.exp(r * dt)
            if i < steps:
                nxt = delta(s, 1.0 - i * dt)
                bank -= (nxt - held) * s
                held = nxt
        pnl = math.exp(-r) * (bank + held * s - max(s - s0, 0.0))
        total += pnl
        total_sq += pnl * pnl
    mean = total / paths
    sd = math.sqrt(total_sq / paths - mean * mean)
    print(
        f"Model risk: one-year ATM call sold at 15% vol ({premium:.4f}) when "
        f"the true vol is 25% ({fair:.4f})"
    )
    print(
        f"  weekly delta hedging at 15%: mean P&L {mean:.4f} (sd {sd:.4f}); "
        f"expected loss from the price gap {premium - fair:.4f}"
    )


def _position_limits(lib: Any) -> None:
    daily_vol, limit = 0.015, 5.0
    position = limited = 10.0
    loss = limited_loss = 0.0
    breach = -1
    max_position = limit / (daily_vol * lib.math.norm_inv(0.99))
    for day in range(1, 11):
        loss += 0.01 * position
        limited_loss += 0.01 * limited
        position *= 2.0
        var = lib.risk.normal_var(daily_vol * position, 0.99)
        if var > limit and breach < 0:
            breach = day
        limited = min(2.0 * limited, max_position)
    print(
        "Position limits: doubling up in a market falling 1% a day for 10 days"
    )
    print(
        f"  VaR limit of $5m first breached on day {breach}; loss {loss:.2f} "
        f"$m unchecked vs {limited_loss:.2f} $m with the limit enforced"
    )


def main() -> None:
    """Runs the chapter's examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    _stack_and_roll()
    _model_risk(lib)
    _position_limits(lib)


if __name__ == "__main__":
    main()
