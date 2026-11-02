# Chapter 14: Wiener processes and Ito's lemma

Wiener and generalized Wiener processes (14.2), geometric Brownian motion
and the Table 14.1 simulation (14.3-14.4), correlated processes (14.5), and
a Monte Carlo check of Ito's lemma for ln S (14.6-14.7). Golden tests:
`data/golden/ch14.json`. Fractional Brownian motion is deferred to M5
alongside the Monte Carlo engine.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/wiener.cc` | `ofod/models/brownian.h`, `ofod/random/normal_generator.h` |
| Python | `py/wiener.py [--cpp]` | `ofod.models`, `ofod.random` |
