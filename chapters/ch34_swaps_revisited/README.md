# Chapter 34: Swaps revisited

Variations on the vanilla swap (34.1): amortizing, step-up and forward
swaps on a general schedule; LIBOR-in-arrears swaps (34.2) and constant
maturity swaps (34.4) with convexity and timing adjustments; a diff swap
through the quanto adjustment; an accrual swap (34.6) from digital
probabilities; and a cancelable swap (34.7) as a swap plus a Bermudan
swaption on a Hull-White tree. Golden tests: `data/golden/ch34.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/swaps.cc` | `ofod/pricers/exotic_swaps.h`, `ofod/pricers/adjustments.h`, `ofod/pricers/short_rate_tree.h` |
| Python | `py/swaps.py [--cpp]` | `ofod.pricers.exotic_swaps`, `ofod.pricers.adjustments`, `ofod.pricers.short_rate_tree` |

Values are to the receiver of the fixed leg and, as in Hull, assume the
(adjusted) forward rates are realised. The CMS rate adds the par-bond
convexity adjustment of Ch 30 to the forward swap rate and multiplies by
the timing factor for paying one period later. The accrual swap counts
each fixing in range with probability N(d2(low)) - N(d2(high)) under
Black. All values are golden-tested values computed by this library
rather than book figures, since Hull's Ch 34 examples are descriptive.
Every line is deterministic: the C++ binary, `py` and `py --cpp` print
the same output.
