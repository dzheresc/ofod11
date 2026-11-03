# Comparison with other libraries

This page has the results of timing `ofod` against the libraries picked in
[the comparison plan](comparison_plan.md). Each case prices the same
instrument with the same inputs in every library that covers it, with
steps, grids, paths and quadrature matched as far as each library allows.
The tables report the time per call and the error against a reference
value, because a faster result that is less accurate is not a like-for-like
win.

Everything here ran in the project's Docker image on a desktop machine
(see [Setup](#setup)). The first runs were in a noisier cloud container;
their raw results stay in `perf/compare-cpp.json` and
`perf/compare-python.json`. The order of the libraries did not change on
the desktop, except that the 1,000-step tree's lead over FinancePy
shrank from 10x to 3.3x (the cloud figure was an outlier), and some
ratios against QuantLib moved.

The first run found three places where ofod was slower: the Heston
integral, the European Monte Carlo and, through the bindings, the 100-step
CRR tree. The tables below are from a second run after the
[gap-closing pass](PERF.md#comparison-follow-up-closing-the-gaps), which
fixed all three; the summary says what changed.

## Summary

- **Trees, finite differences and short-rate trees: ofod is the fastest.**
  In C++ against C++, the 1,000-step CRR tree is 14x faster than
  QuantLib, Crank-Nicolson American finite differences 4.2x, and the
  Hull-White Bermudan swaption tree 112x. The errors match QuantLib's
  to within the discretisation (same tree, same answer to 1e-5). Through
  the bindings, the 100-step tree takes 5.3 us against FinancePy's Numba
  loop at 10 us, with the same prices to the last bit.
- **Heston: ofod is 1.4x faster than QuantLib at the same accuracy**
  (18 us against 26 us in C++, 1e-14 from the reference). It was 12x
  slower before the gap-closing pass. Instead of copying QuantLib's fixed
  Gauss-Laguerre rule, ofod now integrates Lewis's single-integral form
  with a Black-Scholes control variate on panels that widen with the
  integrand's decay, and keeps its adaptive stopping test, so it stays
  accurate for short maturities and extreme parameters where a fixed
  rule is not checked. The Python package uses the same method: 0.38 ms.
- **Monte Carlo: ofod_cpp is 10x faster than QuantLib's European engine
  and 21x faster than its Longstaff-Schwartz engine.** On the European
  case the default generator (`std::mt19937_64` with
  `std::normal_distribution`, 18 ns a draw) still loses to FinancePy's
  Numba loop (3.5 ms against 2.7 ms). The opt-in `Rng::kZiggurat`
  (xoshiro256++ with a ziggurat, 3 ns a draw) takes 1.3 ms, 1.4x faster
  than NumPy-vectorised ofod (Python, 1.7 ms, which also draws with a
  ziggurat) and 2.1x faster than FinancePy. It is opt-in because a new
  generator gives new random numbers: the default keeps every seeded
  result in the golden files.
- **Exposure and CVA: ofod_cpp is 74x faster than ORE on the one trade
  they share, and ORE does far more.** A 10,000-path, 73-date forward
  takes 51 ms against 3.7 s. ORE is a general portfolio engine that
  reprices the trade on every path and date through a simulated market,
  while ofod has a closed-form value for this single product. ORE's CVA
  also sits 9e-3 below the exact value because of its summation rule, about
  one standard deviation of a 10,000-path run; see
  [below](#exposure-and-cva).
- **Closed forms and implied volatility: ofod_cpp is the fastest library
  and as accurate as any.** QuantLib's `blackFormulaImpliedStdDev`
  defaults to an accuracy of 1e-6 (error 5e-7); asked for 1e-12 it takes
  1.6 us against ofod's 0.26 us. vollib's Let's Be Rational is exact but
  pure Python (20 us).
- **CDO tranche and GARCH:** ofod_cpp is 12x faster than FinancePy on the
  tranche spread (the binomial shortcut from M12 against FinancePy's full
  recursion) and 2.3x faster than `arch` on the GARCH(1,1) fit, with a
  slightly higher likelihood under Hull's definition.
- **FinancePy prices with Hull's polynomial approximation to N(x),** the
  one in OFOD that is good to 6 decimals, so its BSM prices are off by
  6e-7 and its implied volatilities by 2e-7. Everything else matched.

Two problems turned up along the way:

1. **ofod bug: `GaussHermite(n)` is wrong for n = 199 and 200.** The
   function accepts n up to 200, but for 199 and 200 every node converges
   to the same root, so the weights sum to about 1e-160 instead of 1 and
   `TrancheSpread` returns about 1e-161. Up to 198 the rule is fine (the
   weights sum to 1 and E[Z^2] = 1). The fix is either to cap n at 198 or
   to fix the initial guesses; it is not part of this change.
2. **FinancePy 1.1.2 bug:** `equity_lsmc` fails with a `NameError` when
   its Numba polynomial fit does not compile, because the NumPy fallback
   names `_eval_poly_numpy`, which does not exist. The harness points that
   name at `_eval_polynomial_numpy` before calling it.

## Setup

| | |
| --- | --- |
| Host | Intel Core Ultra 9 185H, Docker Desktop on WSL2, Ubuntu 24.04 (see [Performance](PERF.md#desktop-pass)) |
| C++ | GCC 13.3, `Release` (-O3), Google Benchmark from vcpkg, median of 5 repetitions |
| QuantLib (C++) | 1.43, from vcpkg |
| Python | 3.12.3, NumPy 2.3.5, Numba 0.62.1, best of 5 timeit runs |
| Python libraries | QuantLib 1.44 (wheel), FinancePy 1.1.2, arch 8.0.0, vollib 1.0.11, ORE 1.8.17.0 (wheel) |
| Threads | One everywhere (`OMP_NUM_THREADS=1`, `NUMBA_NUM_THREADS=1`), pinned to CPU 4 |

QuantLib's C++ library comes from vcpkg's pinned baseline, so it is 1.43
rather than the 1.44 wheel, and the cloud run used Ubuntu's 1.33. The two
give the same prices. Pure-Python ofod is slower here than in the cloud
because of the interpreter (3.12 against 3.13; see
[Performance](PERF.md#c-against-python_1)), so compare it with the other
libraries in the same table rather than with the older figures.

QuantLib works with dates; a 30/360 day count makes T = 5/12, 0.5, 1 and
10 exact, so its year fractions match the other libraries'. FinancePy's
Heston model also takes dates, and plain numbers stand in for them (see
the script). QuantLib timings are for its instrument and engine objects:
each call sets the engine and asks for the NPV, which is how the library
is used. For the BSM price the table also shows the plain `blackFormula`
function.

"First call" in the Python tables is the time of the first call, which
includes Numba's compilation for FinancePy and is excluded from the timed
runs. Here it ranged from under a millisecond to 0.9 s, for FinancePy's
finite-difference code.

## C++: ofod against QuantLib

From `cpp/compare/compare_quantlib.cc`; raw results in
[`perf/compare-cpp-desktop.json`](perf/compare-cpp-desktop.json).

| Case | ofod | QuantLib | ofod speed-up | ofod value | QuantLib value |
| --- | --- | --- | --- | --- | --- |
| BSM call (`blackFormula`) | 24 ns | 26 ns | 1.1x | 4.759422393 | 4.759422393 |
| BSM call (instrument + `AnalyticEuropeanEngine`) | 24 ns | 1.7 us | 71x | | 4.759422393 |
| Implied vol (QuantLib at 1e-12) | 0.26 us | 1.64 us | 6.4x | 0.2345129140 | 0.2345129140 |
| CRR American put, 100 steps | 4.8 us | 54 us | 11x | 4.278059 | 4.278146 |
| CRR American put, 1,000 steps | 0.28 ms | 3.92 ms | 14x | 4.283627 | 4.283636 |
| FD American put, CN 200 x 100, Brennan-Schwartz | 0.30 ms | 1.24 ms | 4.2x | 4.279686 | 4.281504 |
| ... ofod with projection (as QuantLib) | 0.27 ms | | 4.6x | 4.277555 | |
| ... ofod with PSOR | 0.45 ms | | 2.8x | 4.279686 | |
| MC European call, 100,000 draws | 2.9 ms | 30 ms | 10x | 13.099 | 13.019 |
| ... ofod with `Rng::kZiggurat` | 1.21 ms | | 25x | 13.032 | |
| Heston call, Gauss-Laguerre 144 (QL default) | 18 us | 26 us | 1.4x | 9.242521074 | 9.242521074 |
| ... QuantLib adaptive Gauss-Lobatto, 1e-8 | | 60 us | 3.3x | | 9.242521074 |
| ... QuantLib COS | | 46 us | 2.5x | | 9.242521074 |
| Longstaff-Schwartz put, 10,000 paths x 50 dates | 15.7 ms | 324 ms | 21x | 4.458 | 4.434 |
| Hull-White Bermudan swaption, 500 steps | 1.17 ms | 132 ms | 112x | 0.0328026 | 0.0327794 |

Notes:

- **CRR:** QuantLib's tree gives a slightly different price at the same
  step count (it builds the tree on its own time grid, with the same
  number of steps), but the errors against the reference are the same to
  two digits (see the Python table).
- **Finite differences:** QuantLib uses a log-spot grid and projection
  after each step. On the same 200 x 100 budget it is more accurate
  (error 2.7e-3 against ofod's 4.5e-3 with Brennan-Schwartz) because a
  log grid puts more points near the strike. ofod's own log-space grid
  (`FdGrid::log_space`) is the fairer counterpart for a follow-up.
- **Monte Carlo:** QuantLib draws through its general path generator and
  statistics accumulators. All three prices are within 1.5 standard
  errors (0.042) of the closed form 13.041.
- **Heston:** before this pass ofod took 0.68 ms here, with 16-point
  panels of width 1 and Hull's two-term integrand; see
  [Performance](PERF.md#comparison-follow-up-closing-the-gaps).
- **Longstaff-Schwartz:** QuantLib first simulates 10,000 calibration
  paths for the regression and then 10,000 pricing paths, so it does
  twice the simulation; even per path it is about 10x slower.
- **Hull-White:** QuantLib's `HullWhite::tree` fits the drift with a
  numerical root search at every step and rolls back through its generic
  discretised-asset framework, which costs over 100x. Its price is a
  little closer to the finite-difference reference (2.4e-5 against
  4.7e-5, on a price of 0.0328); both trees put nodes on the exercise
  dates, so the difference is in the trees themselves and was not
  investigated further.

## Python: every library

From `tools/compare/compare_python.py`; raw results in
[`perf/compare-python-desktop.json`](perf/compare-python-desktop.json). Errors are against
the reference named for each case. Times include each library's Python
call overhead, so the bindings (ofod_cpp and QuantLib) pay a few hundred
nanoseconds per call.

### BSM price

Call, S = 42, K = 40, r = 10%, sigma = 20%, T = 0.5. Reference: the
formula with `math.erfc`.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 825 ns | 0 |
| ofod_cpp | 170 ns | 0 |
| QuantLib (`blackFormula`) | 279 ns | 9e-16 |
| FinancePy (`european_value`, Numba) | 2.4 us | 6.0e-7 |
| vollib | 4.4 us | -4e-15 |

### Implied volatility

Call at 1.875, S = 21, K = 20, r = 10%, T = 0.25. Reference: Let's Be
Rational (vollib), exact to rounding.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 8.4 us | 1e-15 |
| ofod_cpp | 436 ns | 1e-15 |
| QuantLib (default accuracy 1e-6) | 1.07 us | -5.1e-7 |
| FinancePy (Newton, Numba) | 37 us | -2.2e-7 |
| vollib | 20 us | 0 |

### American put on a CRR tree

S = K = 50, r = 10%, sigma = 40%, T = 5/12. Reference 4.284230: the mean
of 20,000- and 20,001-step CRR trees.

| Library | 100 steps | 1,000 steps | Error at 1,000 |
| --- | --- | --- | --- |
| ofod (Python) | 3.29 ms | 321 ms | -6.0e-4 |
| ofod_cpp | 5.3 us | 0.28 ms | -6.0e-4 |
| QuantLib | 51 us | 3.48 ms | -5.9e-4 |
| FinancePy (Numba) | 10 us | 0.94 ms | -6.0e-4 |

FinancePy runs the same algorithm and returns ofod's price to all digits.
ofod_cpp is 1.9x faster at 100 steps and 3.3x faster at 1,000 steps. The
cloud run read 4.17 ms for FinancePy at 1,000 steps, which made that lead
look like 10x; the desktop's 0.94 ms fits the O(N^2) cost of its 100-step
time.

### American put by Crank-Nicolson finite differences

Same option, 200 price steps x 100 time steps; same reference.

| Library | Method | Time | Error |
| --- | --- | --- | --- |
| ofod (Python) | Brennan-Schwartz | 17.2 ms | -4.5e-3 |
| ofod (Python) | PSOR | 26.5 ms | -4.5e-3 |
| ofod_cpp | Brennan-Schwartz | 313 us | -4.5e-3 |
| ofod_cpp | projection | 267 us | -6.7e-3 |
| ofod_cpp | PSOR | 456 us | -4.5e-3 |
| QuantLib | projection, log grid | 1.22 ms | -2.7e-3 |
| FinancePy (Numba) | projection, log grid | 0.91 ms | -4.4e-3 |
| FinancePy (Numba) | PSOR, log grid | 12.8 ms | -4.4e-3 |

### European call by Monte Carlo

S = 50, K = 40, r = 10%, sigma = 40%, T = 0.5, 100,000 terminal draws.
Reference: the closed form, 13.0407; one standard error is 0.042.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python, NumPy) | 1.73 ms | -0.079 |
| ofod_cpp (default generator) | 3.52 ms | 0.058 |
| ofod_cpp (`Rng.ZIGGURAT`) | 1.27 ms | -0.009 |
| QuantLib | 33 ms | -0.022 |
| FinancePy (Numba loop) | 2.71 ms | 0.017 |

The errors are all within two standard errors; with different random
numbers they are noise, not accuracy. NumPy's `standard_normal` is also a
ziggurat, which is why the vectorised Python was ahead of the C++ default.

### Heston call

S = K = 100, r = 3%, T = 1, v0 = theta = 0.04, kappa = 2, sigma = 0.3,
rho = -0.7. Reference: QuantLib's adaptive Gauss-Lobatto at 1e-13.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 380 us | -1e-14 |
| ofod_cpp | 19.5 us | -1e-14 |
| QuantLib (Gauss-Laguerre 144) | 29.8 us | -4e-15 |
| QuantLib (COS) | 50 us | -5e-12 |
| FinancePy (`value_lewis`, SciPy `quad`) | 0.80 ms | 6e-13 |

Before the gap-closing pass, in the cloud: ofod (Python) 1.85 ms and
ofod_cpp 598 us, both within 8e-14.

### Longstaff-Schwartz

American put, S = 36, K = 40, r = 6%, sigma = 20%, T = 1, 10,000 paths
and 50 exercise dates, regression on 1, S, S^2. Reference 4.4867 (CRR
American; the 50-date Bermudan is a little lower). One standard error is
0.028.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 34 ms | 0.011 |
| ofod_cpp | 18.5 ms | -0.029 |
| QuantLib | 370 ms | -0.052 |
| FinancePy (antithetic pairs) | 53 ms | -0.016 |

### Hull-White Bermudan swaption

Receiver swaption, flat 4% curve, a = 0.1, sigma = 1%, 10-year annual
swap at 4%, exercise on years 1 to 9, 500 tree steps (tree build
included). Reference 0.0327553: QuantLib's finite-difference engine on a
2,000 x 800 grid.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 111 ms | 4.7e-5 |
| ofod_cpp | 1.25 ms | 4.7e-5 |
| QuantLib (`TreeSwaptionEngine`) | 134 ms | 2.4e-5 |

FinancePy's Bermudan swaption takes calendar dates and its own day
counts, so it cannot be set up to match this case exactly and is left
out.

### CDO tranche spread

Mezzanine 3-6%, 125 names, hazard 1%, recovery 40%, correlation 0.3,
5 years quarterly, 64 quadrature points. Reference 4.7175454%: ofod_cpp
with 180 Gauss-Hermite points. QuantLib's Python wheel does not expose its
CDO engines.

| Library | Time | Error (spread) |
| --- | --- | --- |
| ofod (Python) | 68 ms | -1.9e-7 |
| ofod_cpp | 1.32 ms | -1.9e-7 |
| FinancePy (full recursion, Numba) | 16 ms | 7e-9 |

FinancePy's 64-step rectangle rule on [-6, 6] happens to be closer than
64 Gauss-Hermite points here; both errors are below 0.002 bp.

### GARCH(1,1) fit

1,000 returns simulated from a GARCH(1,1) with omega = 2e-6, alpha = 0.1,
beta = 0.88 (seeded). The value is Hull's log likelihood,
sum of -(ln v + u^2 / v) with v_1 = u_0^2, at each library's fitted
parameters; higher is better.

| Library | Time | Log likelihood | omega | alpha | beta |
| --- | --- | --- | --- | --- | --- |
| ofod (Python) | 72 ms | 8140.511 | 2.74e-6 | 0.0995 | 0.8799 |
| ofod_cpp | 1.54 ms | 8140.511 | 2.74e-6 | 0.0995 | 0.8799 |
| arch | 3.5 ms | 8140.306 | 2.50e-6 | 0.1000 | 0.8800 |

`arch` maximises its own likelihood, which starts the variance from a
backcast rather than from u_0^2, so it scores a little lower on Hull's
definition; both recover the true parameters.

### Exposure and CVA

From `tools/compare/compare_ore.py`; raw results in
[`perf/compare-ore.json`](perf/compare-ore.json). ORE 1.8.17.0 ships its own
QuantLib, so this is a separate script, which writes ORE's XML inputs to a
temporary directory.

A long equity forward, S = K = 100, r = 5%, sigma = 30%, T = 3, on 73 dates
15 days apart with 10,000 paths; the counterparty has a flat 2% hazard rate
and 40% recovery, with no collateral. ORE's rate volatility is set to zero,
so its cross-asset model reduces to the geometric Brownian motion that ofod
simulates. T is three years so that ORE's Actual/Actual model time and
Actual/365 curves agree (no leap day), and its trade is 10^6 units because
its reports round to whole currency units.

Discounted expected exposure has a closed form,
S N(d1) - K e^{-rT} N(d2), a Black-Scholes call with strike K e^{-r(T - t)}.
The reference CVA is 0.73637: the integral of that exposure against the
default density, times 1 - R. The exposure errors are measured up to the
last date before maturity. "Same-rule error" is against each library's own
CVA sum applied to the exact exposure, which separates the Monte Carlo
error from the rule.

| Library | Time | CVA | Error | Same-rule error | Max exposure error | RMS exposure error |
| --- | --- | --- | --- | --- | --- | --- |
| ofod (Python) | 67 ms | 0.72163 | -1.5e-2 | -1.5e-2 | 0.78 | 0.46 |
| ofod_cpp | 51 ms | 0.73542 | -9.5e-4 | -9.6e-4 | 0.44 | 0.18 |
| ORE (Mersenne Twister) | 3.74 s | 0.72512 | -1.1e-2 | -1.9e-3 | 0.36 | 0.18 |
| ORE (Sobol, Brownian bridge) | 3.79 s | 0.72565 | -1.1e-2 | -1.4e-3 | 0.075 | 0.045 |

- **Time.** ORE's figure is a complete run: it builds the market, reprices
  the trade through a simulated market on all 730,000 path-dates, and
  writes its reports. ofod computes the forward's value directly and has
  nothing but this product, so 74x says what generality costs, not that
  one engine is better. A third of ofod's own time is the sort for the PFE
  quantile (see [Performance](PERF.md#profiles)).
- **Monte Carlo error.** One run says little about accuracy at this
  size: over 40 seeds, ofod's CVA has a standard deviation of 8e-3 and its
  RMS exposure error averages 0.26 in C++ and 0.29 in Python. ofod_cpp's
  small error above is a lucky seed, and the Python error is 1.7 standard
  deviations. Neither shows a bias (the 40-seed means are within 1e-3).
  ORE's Sobol sequence with a Brownian bridge gets 0.045 at the same cost,
  about 6x better, and ofod has no equivalent for exposure.
- **CVA rule.** ORE's result matches sum q_i EPE(t_i) to the digits it
  prints: each interval uses the exposure at its end. It also values the
  trade at zero on its maturity date, so the last interval adds nothing.
  That explains most of its -1.1e-2 error; about -9e-3 comes from the
  rule, not the simulation. A finer grid shrinks it. ofod's comparison here
  passes interval averages to `Cva`, the midpoint form of eq. 9.1.

## Follow-ups

- Fix `GaussHermite` for 199 and 200 points (or cap it at 198).
- Done in the follow-up pass: the Heston integral, the European Monte
  Carlo (opt-in `Rng::kZiggurat`) and the small CRR tree. The ziggurat
  could also be offered to the path simulator and the other Monte Carlo
  engines; that changes their seeded results only if opted into, so it is
  left until there is a need.
- Finite differences: compare on ofod's log-space grid as well.
- ORE covers more than this one trade (swaps, collateral, initial margin),
  but ofod's exposure code only has the forward, so there is nothing else
  like-for-like to time.
- vcpkg's baseline has QuantLib 1.43; moving to 1.44 needs a newer baseline.

## Reproducing

```bash
# Inside the Docker image: docker compose -f docker/compose.yaml run --rm profile bash
# C++ (vcpkg builds QuantLib)
cmake --preset compare
cmake --build --preset compare --target ofod_compare_quantlib
taskset -c 4 build/compare/cpp/compare/ofod_compare_quantlib \
    --benchmark_repetitions=5 --benchmark_report_aggregates_only=true \
    --benchmark_out=docs/perf/compare-cpp-desktop.json \
    --benchmark_out_format=json

# Python (ofod and ofod_cpp installed)
pip install -r tools/compare/requirements.txt
OMP_NUM_THREADS=1 NUMBA_NUM_THREADS=1 taskset -c 4 \
    python tools/compare/compare_python.py \
    --out docs/perf/compare-python-desktop.json
OMP_NUM_THREADS=1 taskset -c 4 \
    python tools/compare/compare_ore.py --out docs/perf/compare-ore.json
```
