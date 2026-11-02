# Chapter 20: Volatility smiles and volatility surfaces

Volatility surfaces (20.6), with Hull's Table 20.2 interpolated linearly in
volatility, as the book does, or in total variance sigma^2 T; smiles quoted
in delta space; the minimum variance delta (20.5); the risk-neutral density
implied by a smile, via Breeden-Litzenberger (appendix); and the frown that
a single anticipated large jump produces (20.8). Golden tests:
`data/golden/ch20.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/smiles.cc` | `ofod/curves/vol_surface.h`, `ofod/pricers/smile.h` |
| Python | `py/smiles.py [--cpp]` | `ofod.curves.VolSurface`, `ofod.pricers.smile` |

The skewed smile in the density example, sigma(K) = 0.25 - 0.1 (K/S0 - 1),
is illustrative, not from the book. The density it implies has a fatter
left tail than the 25% lognormal, as Hull's Figure 20.4 shows for equities.
