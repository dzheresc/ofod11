# Chapter 27: More on models and numerical procedures

Alternatives to Black-Scholes-Merton and the numerical methods they
need: CEV via the noncentral chi-square (27.1), Merton's jump diffusion
as a Poisson mixture of BSM prices (27.1), Heston stochastic volatility
by a Fourier integral and by full-truncation Monte Carlo (27.2),
Longstaff-Schwartz least-squares Monte Carlo for American options
(27.8) and Hull's convertible bond tree with default (Example 27.1).
Golden tests: `data/golden/ch27.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/models.cc` | `ofod/pricers/model_pricers.h`, `ofod/pricers/american_mc.h` |
| Python | `py/models.py [--cpp]` | `ofod.pricers.model_pricers`, `ofod.pricers.american_mc` |

The convertible tree gives Hull's 106.93 with 3 steps and converges to
about 106.1 as steps grow. The LSM price for Longstaff and Schwartz's
Table 1 case (4.478) agrees with a 2000-step tree (4.4867) within a few
standard errors; it uses a polynomial basis of degree 2 in the spot.

Notes on the implementation:

- The Heston integral uses the "little trap" characteristic function and
  composite 16-point Gauss-Legendre panels of width 1, stopping after
  three negligible panels. The plan named Gauss-Laguerre; panels proved
  more robust for short maturities and high vol-of-vol. See
  `docs/PERF.md` for the profiling that halved its cost.
- CEV is undefined at alpha = 1 and numerically unstable close to it, so
  `|alpha - 1| < 0.01` returns `std::nullopt` / raises `ValueError`; use
  BSM there.
- The Heston Monte Carlo and LSM lines differ between C++ and pure
  Python because mt19937_64 and NumPy draw different numbers; they agree
  within Monte Carlo error. `py --cpp` matches the C++ output exactly.
