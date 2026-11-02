# Chapter 26: Exotic options

Closed forms for the exotics of Hull Ch 26: gap options (Example 26.1),
forward starts, perpetual Americans, binaries, Geske compound options,
simple choosers, Reiner-Rubinstein barriers, floating and fixed
lookbacks, geometric Asians and arithmetic Asians by moment matching
(Example 26.3), Margrabe exchange options and the fair variance of a
variance swap from a strip of options (26.16). Golden tests:
`data/golden/ch26.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/exotics.cc` | `ofod/pricers/exotics.h` |
| Python | `py/exotics.py [--cpp]` | `ofod.pricers.exotics` |

Book values reproduced: gap put 1895.69, floating lookbacks 8.04 and
7.79, Asians 5.62 (arithmetic) and 5.13 (geometric). The other golden
values are marked "computed" and are checked with parities
(in + out = vanilla, put-call parity for compounds) and with Monte Carlo
in the unit tests; the discretely monitored barrier test uses the
Broadie-Glasserman-Kou shift of 0.5826 sigma sqrt(dt).

Lookbacks need r != q (the closed form divides by r - q), so they
return `std::nullopt` / raise `ValueError` otherwise. The variance swap
integrates the option strip with composite Gauss-Legendre over the
given strike range, so truncation shows up as the small gap between
25.01% and the flat 25% input. Every line is deterministic: the C++
binary, `py` and `py --cpp` print the same output.
