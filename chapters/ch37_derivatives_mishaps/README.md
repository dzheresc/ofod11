# Chapter 37: Derivatives mishaps and what we can learn from them

Three of Hull's lessons replayed with code from earlier chapters, rather
than new library code:

1. **Liquidity and hedging (Metallgesellschaft).** Fixed-price delivery
   contracts for five years, hedged with a stack of one-month futures that
   is rolled each month. When prices fall, the futures losses are paid in
   cash every month while the offsetting gain on the delivery contracts is
   only a mark-to-model value (Ch 2 margin, Ch 3 stack and roll).
2. **Model risk.** Options sold at 15% implied volatility when the true
   volatility is 25%, then delta hedged at 15%. The hedging losses average
   the gap between the two Black-Scholes-Merton prices (Ch 15, Ch 19).
3. **Risk limits.** A trader doubles a losing position each day. A 99%
   one-day VaR limit (Ch 22) would have stopped the position on day 4.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/mishaps.cc` | `ofod/pricers/black_scholes.h`, `ofod/risk/var.h`, `ofod/random/normal_generator.h` |
| Python | `py/mishaps.py [--cpp]` | `ofod.pricers`, `ofod.risk`, `ofod.random` |

The C++ binary and `py --cpp` print the same output. The pure-Python run
differs on the model-risk Monte Carlo line only.
