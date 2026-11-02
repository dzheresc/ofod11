# Sample market data

Synthetic series used by the chapter examples, so that C++, Python and
Python `--cpp` read identical inputs. Each file starts with a `#` comment
describing how it was generated.

| File | Contents | Used by |
| --- | --- | --- |
| `garch_returns.csv` | 1500 daily returns simulated from GARCH(1,1) with omega 2e-6, alpha 0.13, beta 0.86 | Ch 22, Ch 23 examples; `data/golden/ch23.json` |
| `rate_changes.csv` | 500 days of changes in four rates, two factors plus noise | Ch 22 PCA example; `data/golden/ch22.json` |
