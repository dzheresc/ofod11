# Chapter 35: Energy and commodity derivatives

A seasonal natural gas curve fitted to monthly futures (35.3); Hull's
trinomial tree for a mean-reverting commodity price, shifted so the
expected price at each step equals the futures price (35.4), with options
on the spot price; a mean-reverting log-price with jumps for electricity
spikes; heating and cooling degree days and degree-day options priced by
burn analysis and by simulating a fitted seasonal temperature model
(35.5); a catastrophe bond and an excess-of-loss reinsurance layer on a
compound Poisson loss process (35.6); and a producer's hedge with a
forward sale or a collar. Golden tests: `data/golden/ch35.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/commodities.cc` | `ofod/models/commodity.h`, `ofod/pricers/commodity_tree.h`, `ofod/pricers/weather_insurance.h` |
| Python | `py/commodities.py [--cpp]` | `ofod.models.commodity`, `ofod.pricers.commodity_tree`, `ofod.pricers.weather_insurance` |

The commodity tree reuses Hull's branching from the Hull-White tree of
Ch 32 (`HullBranch`), with pure probabilities in place of Arrow-Debreu
prices. The spike model's expected price is closed form for the
Ornstein-Uhlenbeck part plus a Gauss-Legendre integral for the jumps, and
the simulation samples both exactly, so the two agree within Monte Carlo
error. Weather and catastrophe risks are taken to be uncorrelated with the
market and priced as discounted real-world expectations, as Hull argues.
The temperature series is synthetic and deterministic. Values are computed
by this library; Hull's Ch 35 examples are mostly descriptive.

The C++ binary and `py --cpp` print the same output. The pure-Python run
differs on the Monte Carlo lines (spikes, winter HDD, cat bond and layer),
since it draws from NumPy rather than mt19937_64.
