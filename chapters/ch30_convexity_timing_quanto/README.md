# Chapter 30: Convexity, timing and quanto adjustments

The convexity adjustment for a payoff equal to a bond yield or swap rate
(30.1), the timing adjustment for a payment made later than the market
convention (30.2), the quanto adjustment for a variable paid in another
currency (30.3), and Siegel's paradox. Golden tests:
`data/golden/ch30.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/adjustments.cc` | `ofod/pricers/adjustments.h` |
| Python | `py/adjustments.py [--cpp]` | `ofod.pricers.adjustments` |

Book values reproduced: Example 30.1 (adjustment 0.00109, value 75.95)
and Example 30.3, the same payoff made a year later (value 68.30). The
convexity adjustment uses analytic G'(y) and G''(y) of a bond priced
from its yield; a swap rate is the yield of a par bond. The timing
adjustment is applied as the exponential factor of eq. 30.4.

Siegel's paradox: if S is the USD price of 1 EUR, the forward 1/F is not
E[1/S_T] under the USD measure; it is under the EUR measure, which
changes the drift by sigma^2. The Monte Carlo line differs between C++
and pure Python because mt19937_64 and NumPy draw different numbers;
`py --cpp` matches the C++ output exactly.
