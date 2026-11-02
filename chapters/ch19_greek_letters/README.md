# Chapter 19: The Greek letters

Delta, gamma, theta, vega and rho for European options in closed form
(Table 19.6), the same Greeks from a generic central-difference bumper that
works with any pricer, gamma neutral and gamma-vega neutral hedges (19.6,
19.7), scenario analysis (19.9) and the delta-hedging experiment behind
Hull's hedging-performance table (19.4). Golden tests:
`data/golden/ch19.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/greeks.cc` | `ofod/greeks/greeks.h` |
| Python | `py/greeks.py [--cpp]` | `ofod.greeks` |

The library reports theta per year, vega per unit of volatility and rho per
unit of rate; the example also prints Hull's per-day and per-1% figures.
The hedging-performance lines agree between C++ and Python only
statistically (`std::mt19937_64` vs NumPy's PCG64). Hull's 0.34 for
4-week rebalancing is off the 1/sqrt(n) line that the other entries follow
(0.37 expected); the simulation here gives 0.37.
