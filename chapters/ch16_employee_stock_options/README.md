# Chapter 16: Employee stock options

The Hull-White binomial model for employee stock options (16.3): a CRR tree
with a vesting period, an early-exercise multiple M (exercise once
S >= M K) and an employee exit rate, with leavers forfeiting unvested
options and exercising vested ones. The expected-life approach (16.4) and
dilution, valuing ESOs as warrants (16.5). Golden tests:
`data/golden/ch16.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/eso.cc` | `ofod/pricers/eso.h`, `ofod/pricers/black_scholes.h` |
| Python | `py/eso.py [--cpp]` | `ofod.pricers.eso` |

The exit probability over a step of length dt is 1 - e^{-exit_rate dt}.
