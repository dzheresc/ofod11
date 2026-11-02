# Chapter 24: Credit risk

Hazard rates from credit spreads and cumulative default probabilities
(24.1-24.3), a piecewise-constant hazard curve, Merton's model with Hull's
E0 = 3, sigma_E = 80%, D = 10 example (24.6), the Gaussian copula for joint
and conditional defaults (24.8), and the Vasicek worst-case default rate
and credit VaR for a retail loan portfolio (24.9). Golden tests:
`data/golden/ch24.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/credit.cc` | `ofod/credit/credit_risk.h`, `ofod/curves/credit_curve.h`, `ofod/math/normal.h` |
| Python | `py/credit.py [--cpp]` | `ofod.credit.credit_risk`, `ofod.curves.credit_curve`, `ofod.math.normal` |

Merton's two equations are solved by a two-dimensional Newton iteration and
reproduce Hull's V0 = 12.40, sigma_V = 21.23% and 12.7% default
probability. The bivariate normal CDF is a port of Genz's BVND algorithm in
both languages, so the joint default probabilities are identical. The
output is the same from C++, Python and `py --cpp`.
