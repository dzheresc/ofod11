# Chapter 9: XVAs

Credit value adjustment on discrete default intervals (9.2), the exposure
profile of a forward contract simulated under geometric Brownian motion,
with expected exposure (EE), expected negative exposure (ENE) and 97.5%
potential future exposure (PFE), the effect of a collateral threshold
(9.4), the analytic CVA of a forward when default can occur only at
maturity (9.8), DVA (9.6) and FVA (9.7). Golden tests: `data/golden/ch09.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/xva.cc` | `ofod/credit/xva.h`, `ofod/curves/credit_curve.h` |
| Python | `py/xva.py [--cpp]` | `ofod.credit.xva`, `ofod.curves.credit_curve` |

Exposures are discounted to today, so CVA is (1 - R) times the sum of the
interval default probabilities times EE. A collateral threshold H caps the
exposure at min(max(V, 0), H). The simulated lines agree between C++ and
pure Python only within Monte Carlo error (`std::mt19937_64` vs NumPy);
`py --cpp` reproduces the C++ output exactly.
