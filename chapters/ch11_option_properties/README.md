# Chapter 11: Properties of stock options

Upper and lower bounds (11.3-11.4), put-call parity for European and
American options (11.4), early exercise of calls and puts (11.5-11.6) and
the effect of dividends (11.7). Golden tests: `data/golden/ch11.json`.
The bounds double as invariants: `python/tests/test_options.py` checks
that every binomial tree price satisfies them.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/bounds.cc` | `ofod/pricers/bounds.h` |
| Python | `py/bounds.py [--cpp]` | `ofod.pricers.bounds` |
