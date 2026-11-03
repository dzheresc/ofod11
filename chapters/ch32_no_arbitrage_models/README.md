# Chapter 32: Interest rate derivatives: models of the short rate fitted to today's curve

Ho-Lee and Hull-White fitted to the zero curve (32.2, 32.3), bond
options in closed form and by Jamshidian's decomposition, Hull's
two-stage trinomial tree (32.7) with Black-Karasinski on the same
grid in ln r, European and Bermudan swaptions and American bond options
on the tree (32.8), and calibration to caplet prices (32.10). Golden
tests: `data/golden/ch32.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/trees.cc` | `ofod/models/short_rate.h`, `ofod/pricers/short_rate_tree.h` |
| Python | `py/trees.py [--cpp]` | `ofod.models.short_rate`, `ofod.pricers.short_rate_tree` |

Book values reproduced: Hull's tree for a = 0.1, sigma = 0.01 and
dt = 1 on his zero curve, with dx = 0.01732, jmax = 2, the branching
probabilities at the edges (0.8867, 0.0267, 0.0867) and alpha = 3.824%,
5.205% and 6.252%.

The tree uses M = -a dt and the dt-period rate at each node, as in Hull,
so it converges to the analytic Hull-White prices at first order in dt,
not monotonically. Calibration fits a and sigma by Nelder-Mead on
relative errors of caplet prices; Black vols that are flat across
strikes leave a residual because Hull-White produces a skew. Every line
is deterministic: the C++ binary, `py` and `py --cpp` print the same
output.
