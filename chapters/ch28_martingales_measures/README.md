# Chapter 28: Martingales and measures

The market price of risk (28.1) and the change of numeraire (28.3-28.5):
a call's expected return equals r + lambda sigma_c, and pricing the same
call under the money-market, stock and zero-coupon bond numeraires gives
the BSM value within Monte Carlo error. The exchange option of Ch 26 is
priced again with the U asset as numeraire, where it becomes a plain
call on the ratio V/U (28.6). There are no golden tests: the chapter's
results are identities, which the example checks numerically.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/measures.cc` | `ofod/greeks/greeks.h`, `ofod/pricers/exotics.h` |
| Python | `py/measures.py [--cpp]` | `ofod.greeks`, `ofod.pricers.exotics` |

The Monte Carlo lines differ between C++ and pure Python because
mt19937_64 and NumPy draw different numbers; `py --cpp` matches the C++
output exactly.
