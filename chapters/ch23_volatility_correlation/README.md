# Chapter 23: Estimating volatilities and correlations

EWMA (23.3), GARCH(1,1) (23.4), maximum likelihood estimation with
Nelder-Mead and the Ljung-Box test on u^2 / sigma^2 (23.5), the GARCH
volatility term structure (23.6), EWMA covariances and correlations (23.7)
and the positive semi-definite check (23.8). Golden tests:
`data/golden/ch23.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/volatility.cc` | `ofod/stats/volatility.h`, `ofod/math/optimize.h` |
| Python | `py/volatility.py [--cpp]` | `ofod.stats.volatility`, `ofod.math.optimize` |

Hull fits GARCH(1,1) to S&P 500 data that is not redistributable; the
example fits returns simulated from known parameters
(`data/market/garch_returns.csv`) and recovers them. Nelder-Mead runs the
same steps in C++ and Python, so all three runs print identical fits.
