# Chapter 21: Basic numerical procedures

Binomial trees for American options (21.1), with dividends (21.3) and the
control variate technique (21.3), trinomial trees (21.4), Monte Carlo
simulation with the variance reduction procedures of 21.7 (antithetic,
control variate, importance sampling, stratified, moment matching and Sobol
quasi-random sequences), and the implicit, explicit and Crank-Nicolson finite
difference methods (21.8), on Hull's American put example. Golden tests:
`data/golden/ch21.json`. Profiling of these engines: `docs/PERF.md`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/numerical.cc` | `ofod/pricers/{binomial_tree,monte_carlo,finite_difference}.h`, `ofod/random/sobol.h` |
| Python | `py/numerical.py [--cpp]` | `ofod.pricers`, `ofod.random.Sobol` |

The tree and finite difference lines are identical in all three runs. The
Monte Carlo lines agree only statistically: C++ draws from `std::mt19937_64`
and Python from NumPy's PCG64, so the estimates differ within their standard
errors.

Importance sampling matches plain Monte Carlo here on purpose: the call is in
the money, and the shift is applied only to out-of-the-money options, where
it reduces variance.
