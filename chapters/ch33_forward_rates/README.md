# Chapter 33: HJM, LMM and multiple zero curves

The Heath-Jarrow-Morton framework in discrete form (33.1), the LIBOR
market model under the rolling spot measure (33.2): caplet vols to
stationary Lambdas, caplet consistency, ratchet and sticky caps with one
and two factors and Rebonato's swaption vol, and agency mortgage-backed
securities with PSA and rate-dependent prepayments (33.4). Golden tests:
`data/golden/ch33.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/forward_rates.cc` | `ofod/models/lmm.h`, `ofod/pricers/mbs.h` |
| Python | `py/forward_rates.py [--cpp]` | `ofod.models.lmm`, `ofod.pricers.mbs` |

Book values reproduced: caplet vols of 24%, 22% and 20% give Lambda =
24%, 19.80% and 15.23%.

Notes on the implementation:

- HJM uses the discrete no-arbitrage drift on its grid, so simulated
  zero-coupon bonds are martingales exactly; with v(t, T) = sigma
  e^{-a(T - t)} it reproduces Hull-White bond option prices.
- The LMM simulates ln F with Hull's drift and an optional
  predictor-corrector step. Payoffs see the whole path (every F_k(t_j)
  and the bank account), return their value in units of the numeraire,
  and are written with NumPy ufuncs so the same function runs on all
  paths at once in pure Python and on one path at a time through the
  bindings.
- The MBS Monte Carlo simulates Hull-White exactly on a monthly grid and
  ties prepayments to a refinancing incentive; the pass-through gains
  less when rates fall than it loses when they rise (negative
  convexity), and the IO strip rises with rates.
- The HJM, LMM and MBS Monte Carlo lines differ between C++ and pure
  Python because mt19937_64 and NumPy draw different numbers; `py --cpp`
  matches the C++ output exactly. `docs/PERF.md` has the LMM profiling.
