# Chapter 22: Value at risk and expected shortfall

VaR and ES for normal losses (22.1), historical simulation with Hull's
"k-th worst of n" convention and volatility scaling (22.2, 22.3), the
linear model with Hull's Microsoft and AT&T example (22.4), the quadratic
model with Cornish-Fisher and Monte Carlo full revaluation (22.6),
backtesting with Kupiec's test (22.8) and principal components analysis
(22.9). Golden tests: `data/golden/ch22.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/var.cc` | `ofod/risk/var.h`, `ofod/math/linalg.h` |
| Python | `py/var.py [--cpp]` | `ofod.risk.var`, `ofod.math.linalg` |

Hull's historical data sets are not redistributable, so the example uses
the synthetic series in `data/market/`. The PCA uses a cyclic Jacobi
eigen-solver in both languages (the plan suggested Eigen; a 4x4 symmetric
matrix does not need it, and one algorithm in both languages keeps the
results identical). The Monte Carlo VaR agrees between C++ and Python only
statistically (`std::mt19937_64` vs NumPy).
