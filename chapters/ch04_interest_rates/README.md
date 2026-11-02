# Chapter 4: Interest rates

Compounding (4.4), zero rates and bond pricing (4.5-4.6), bootstrapping
(4.7), forward rates (4.8), FRAs (4.9), duration and convexity (4.10-4.11).
The example reproduces Tables 4.1 and 4.3-4.6; the same numbers are golden
tests in `data/golden/ch04.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/interest_rates.cc` | `ofod/core/compounding.h`, `ofod/curves/`, `ofod/pricers/bond_pricer.h`, `ofod/pricers/fra_pricer.h` |
| Python | `py/interest_rates.py [--cpp]` | `ofod.core`, `ofod.curves`, `ofod.pricers` |
