# Chapter 17: Options on stock indices and currencies

Options on assets paying a known yield (17.1-17.3), index options and
portfolio insurance (17.2), and currency options with the Garman-Kohlhagen
formula (17.4). American index and currency options reuse the binomial
tree with q set to the dividend yield or foreign rate. Golden tests:
`data/golden/ch17.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/index_currency.cc` | `ofod/pricers/black_scholes.h`, `ofod/pricers/binomial_tree.h` |
| Python | `py/index_currency.py [--cpp]` | `ofod.pricers` |
