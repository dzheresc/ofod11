# Chapter 15: The Black-Scholes-Merton model

The lognormal property (15.1-15.3), estimating volatility from historical
data (15.4), the BSM formulas and the cumulative normal (15.8), warrants
(15.10), implied volatility (15.11) and dividends, including Black's
approximation for American calls (15.12). Golden tests:
`data/golden/ch15.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/black_scholes.cc` | `ofod/pricers/black_scholes.h`, `ofod/stats/descriptive.h` |
| Python | `py/black_scholes.py [--cpp]` | `ofod.pricers.black_scholes`, `ofod.stats` |
