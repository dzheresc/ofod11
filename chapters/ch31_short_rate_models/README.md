# Chapter 31: Interest rate derivatives: models of the short rate

Equilibrium short-rate models: Vasicek and Cox-Ingersoll-Ross bond
prices and the term structures they imply (31.3), a Vasicek option on a
zero-coupon bond, parameter estimation from a short-rate series (31.4)
and exact CIR simulation with noncentral chi-square transitions.
Golden tests: `data/golden/ch31.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/short_rate.cc` | `ofod/models/short_rate.h` |
| Python | `py/short_rate.py [--cpp]` | `ofod.models.short_rate` |

The estimator fits the exact AR(1) discretisation of Vasicek by least
squares, which is also the maximum likelihood estimate. With ten years
of daily data the long-run level and volatility come out close, but the
mean reversion a is poorly determined (its standard error is about
sqrt(2a / T) = 0.25 here), which is why practitioners calibrate a to
option prices rather than to history.

The Vasicek fit and CIR simulation lines differ between C++ and pure
Python because mt19937_64 and NumPy draw different numbers (the CIR
transitions use std::gamma_distribution and std::poisson_distribution in
C++ and NumPy's noncentral_chisquare in Python); `py --cpp` matches the
C++ output exactly.
