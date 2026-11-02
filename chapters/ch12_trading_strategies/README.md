# Chapter 12: Trading strategies involving options

Principal-protected notes (12.1-12.2), spreads: bull, bear, box and
butterfly (12.3), and combinations: straddle, strip, strap and strangle
(12.4), plus replication of any piecewise-linear payoff (12.5). Calendar
and diagonal spreads need option values before expiry and arrive with
Black-Scholes in M4. Golden tests: `data/golden/ch12.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/strategies.cc` | `ofod/strategies/strategies.h`, `ofod/instruments/portfolio.h` |
| Python | `py/strategies.py [--cpp] [--plot]` | `ofod.strategies`, `ofod.instruments.portfolio` |
