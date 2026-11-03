# Chapter 29: Standard market models

Black's model for European bond options (29.1), caps and floors as
portfolios of caplets (29.2), European swaptions on the forward swap
rate and its annuity (29.3), the normal (Bachelier) model for low and
negative rates (29.4), and hedging with bucketed DV01s from zero-curve
bumps (29.5). Golden tests: `data/golden/ch29.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/market_models.cc` | `ofod/pricers/rate_options.h` |
| Python | `py/market_models.py [--cpp]` | `ofod.pricers.rate_options` |

Book values reproduced: the bond option of Example 29.1 (forward price
939.68, call 9.49) and the swaption of Example 29.3 ($2.07 million). The
caplet with the inputs of Example 29.2 is 5.19 here, from discounting
at P(0, 1.25) = e^{-0.065 x 1.25}.

Caps follow Hull's convention: the caplet whose rate is known today is
left out, so a 5-year quarterly cap has 19 caplets. Spot vols are
stripped by solving, cap by cap, for the vol of the one new caplet; they
swing more than the flat vols they come from. Bucketed DV01s bump one
pillar of the zero curve at a time, and their sum is close to, but not
exactly, the parallel DV01 because the curve is linearly interpolated.
Every line is deterministic: the C++ binary, `py` and `py --cpp` print
the same output.
