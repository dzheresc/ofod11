# Chapter 36: Real options

The market price of risk from CAPM and the risk-neutral growth rate of a
variable (36.2); risk-neutral valuation of expected cash flows, which
matches discounting at the CAPM rate (36.3); a business valued by Monte
Carlo with revenue uncertainty and bankruptcy when cash runs out (36.4);
the options to abandon, expand, contract and defer on a lattice for the
project's value (36.5); and a resource project valued on the commodity
tree of Ch 35, with and without the options to abandon and expand.
Golden tests: `data/golden/ch36.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/real_options.cc` | `ofod/pricers/real_options.h`, `ofod/pricers/commodity_tree.h` |
| Python | `py/real_options.py [--cpp]` | `ofod.pricers.real_options`, `ofod.pricers.commodity_tree` |

In C++ the project options are a `std::variant<Abandon, Expand,
Contract>`, and in Python a union of dataclasses. Options are exercised at
most once: exercising any of them ends the flexibility. The holder of the
project receives its payout, so with no options the lattice returns the
project value exactly, and the option to abandon is an American put on
the project. The business values rise with volatility because equity
holders walk away from a failed business, which is itself an option.

The C++ binary and `py --cpp` print the same output. The pure-Python run
differs on the business Monte Carlo lines only.
