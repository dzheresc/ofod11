# Chapter 13: Binomial trees

One-step no-arbitrage and risk-neutral valuation (13.1-13.2), two-step
trees (13.3-13.4), puts and American options (13.5-13.6), delta (13.7),
u and d from volatility (13.8), and options on indices, currencies and
futures via the yield q (13.11). The convergence table and `--plot` show
the odd/even oscillation of CRR and the smoother Tian scheme. Golden
tests: `data/golden/ch13.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/binomial_trees.cc` | `ofod/pricers/binomial_tree.h` |
| Python | `py/binomial_trees.py [--cpp] [--plot]` | `ofod.pricers.binomial_tree` |
