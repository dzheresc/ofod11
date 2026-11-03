# Comparison with other libraries

This page has the results of timing `ofod` against the libraries picked in
[the comparison plan](comparison_plan.md). Each case prices the same
instrument with the same inputs in every library that covers it, with
steps, grids, paths and quadrature matched as far as each library allows.
The tables report the time per call and the error against a reference
value, because a faster result that is less accurate is not a like-for-like
win.

Everything here ran in the cloud container. ORE (exposure and CVA) and
desktop timings need the local Docker machine and are left for later.

## Summary

- **Trees, finite differences and short-rate trees: ofod is the fastest.**
  In C++ against C++, the 1,000-step CRR tree is 4.1x faster than
  QuantLib, Crank-Nicolson American finite differences 3.3x, and the
  Hull-White Bermudan swaption tree 100x. The errors match QuantLib's
  to within the discretisation (same tree, same answer to 1e-5).
- **Heston: QuantLib is 11-12x faster at the same accuracy.** Its default
  engine integrates with a fixed 144-point Gauss-Laguerre rule (54 us);
  ofod's adaptive 16-point panels with a tail test take 0.65 ms. Both are
  within 1e-13 of the reference. This is the one place to borrow from.
- **Monte Carlo: ofod_cpp is 17x faster than QuantLib's European engine
  and 37x faster than its Longstaff-Schwartz engine,** whose generic path
  and regression machinery costs about 0.8 us per path step. FinancePy's
  Numba loop beats ofod_cpp on the European case (3.8 ms against
  5.8 ms) for the reason already noted in [Performance](PERF.md): one
  `std::normal_distribution` draw at a time. NumPy-vectorised ofod
  (Python) is faster still, at 1.9 ms.
- **Closed forms and implied volatility: ofod_cpp is the fastest library
  and as accurate as any.** QuantLib's `blackFormulaImpliedStdDev`
  defaults to an accuracy of 1e-6 (error 5e-7); asked for 1e-12 it takes
  2.6 us against ofod's 0.43 us. vollib's Let's Be Rational is exact but
  pure Python (17 us).
- **CDO tranche and GARCH:** ofod_cpp is 12x faster than FinancePy on the
  tranche spread (the binomial shortcut from M12 against FinancePy's full
  recursion) and 2x faster than `arch` on the GARCH(1,1) fit, with a
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
| Host | Cloud container, 4 cores at 2.1 GHz (noisy; see [Performance](PERF.md)) |
| C++ | GCC 13.3, `Release` (-O3), Google Benchmark 1.8.3, median of 5 repetitions |
| QuantLib (C++) | 1.33, Ubuntu 24.04 package `libquantlib0-dev` |
| Python | 3.13, NumPy 2.3.5, Numba 0.62.1, best of 5 timeit runs |
| Python libraries | QuantLib 1.44 (wheel), FinancePy 1.1.2, arch 8.0.0, vollib 1.0.11 |
| Threads | One everywhere (`OMP_NUM_THREADS=1`, `NUMBA_NUM_THREADS=1`) |

QuantLib's C++ library comes from Ubuntu rather than vcpkg, so it is 1.33
rather than the 1.44 wheel; none of the engines used here changed in
substance between the two, and the two give the same prices.

QuantLib works with dates; a 30/360 day count makes T = 5/12, 0.5, 1 and
10 exact, so its year fractions match the other libraries'. FinancePy's
Heston model also takes dates, and plain numbers stand in for them (see
the script). QuantLib timings are for its instrument and engine objects:
each call sets the engine and asks for the NPV, which is how the library
is used. For the BSM price the table also shows the plain `blackFormula`
function.

"First call" in the Python tables is the time of the first call, which
includes Numba's compilation for FinancePy and is excluded from the timed
runs. With Numba's on-disk cache warm it is up to 14 ms; the very first
run in a fresh environment compiled for up to 0.8 s.

## C++: ofod against QuantLib

From `cpp/compare/compare_quantlib.cc`; raw results in
[`perf/compare-cpp.json`](perf/compare-cpp.json).

| Case | ofod | QuantLib | ofod speed-up | ofod value | QuantLib value |
| --- | --- | --- | --- | --- | --- |
| BSM call (`blackFormula`) | 41 ns | 47 ns | 1.1x | 4.759422393 | 4.759422393 |
| BSM call (instrument + `AnalyticEuropeanEngine`) | 41 ns | 4.7 us | 115x | | 4.759422393 |
| Implied vol (QuantLib at 1e-12) | 0.43 us | 2.6 us | 6.1x | 0.2345129140 | 0.2345129140 |
| CRR American put, 100 steps | 18 us | 83 us | 4.6x | 4.278059 | 4.278146 |
| CRR American put, 1,000 steps | 1.34 ms | 5.53 ms | 4.1x | 4.283627 | 4.283636 |
| FD American put, CN 200 x 100, Brennan-Schwartz | 0.54 ms | 1.77 ms | 3.3x | 4.279686 | 4.281504 |
| ... ofod with projection (as QuantLib) | 0.39 ms | | 4.5x | 4.277555 | |
| ... ofod with PSOR | 0.78 ms | | 2.3x | 4.279686 | |
| MC European call, 100,000 draws | 4.7 ms | 81 ms | 17x | 13.099 | 13.019 |
| Heston call, Gauss-Laguerre 144 (QL default) | 0.68 ms | 54 us | 0.08x | 9.242521074 | 9.242521074 |
| ... QuantLib adaptive Gauss-Lobatto, 1e-8 | | 100 us | 0.15x | | 9.242521074 |
| ... QuantLib COS | | 65 us | 0.10x | | 9.242521074 |
| Longstaff-Schwartz put, 10,000 paths x 50 dates | 23.5 ms | 882 ms | 37x | 4.458 | 4.434 |
| Hull-White Bermudan swaption, 500 steps | 1.14 ms | 115 ms | 100x | 0.0328026 | 0.0327794 |

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
  statistics accumulators. Both prices are within 1.5 standard errors
  (0.042) of the closed form 13.041.
- **Longstaff-Schwartz:** QuantLib first simulates 10,000 calibration
  paths for the regression and then 10,000 pricing paths, so it does
  twice the simulation; even per path it is about 19x slower.
- **Hull-White:** QuantLib's `HullWhite::tree` fits the drift with a
  numerical root search at every step and rolls back through its generic
  discretised-asset framework, which costs about 100x. Its price is a
  little closer to the finite-difference reference (2.4e-5 against
  4.7e-5, on a price of 0.0328); both trees put nodes on the exercise
  dates, so the difference is in the trees themselves and was not
  investigated further.

## Python: every library

From `tools/compare/compare_python.py`; raw results in
[`perf/compare-python.json`](perf/compare-python.json). Errors are against
the reference named for each case. Times include each library's Python
call overhead, so the bindings (ofod_cpp and QuantLib) pay a few hundred
nanoseconds per call.

### BSM price

Call, S = 42, K = 40, r = 10%, sigma = 20%, T = 0.5. Reference: the
formula with `math.erfc`.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 583 ns | 0 |
| ofod_cpp | 121 ns | 0 |
| QuantLib (`blackFormula`) | 246 ns | 9e-16 |
| FinancePy (`european_value`, Numba) | 2.8 us | 6.0e-7 |
| vollib | 3.6 us | -4e-15 |

### Implied volatility

Call at 1.875, S = 21, K = 20, r = 10%, T = 0.25. Reference: Let's Be
Rational (vollib), exact to rounding.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 6.7 us | 1e-15 |
| ofod_cpp | 450 ns | 1e-15 |
| QuantLib (default accuracy 1e-6) | 1.25 us | -5.1e-7 |
| FinancePy (Newton, Numba) | 45 us | -2.2e-7 |
| vollib | 17 us | 0 |

### American put on a CRR tree

S = K = 50, r = 10%, sigma = 40%, T = 5/12. Reference 4.284230: the mean
of 20,000- and 20,001-step CRR trees.

| Library | 100 steps | 1,000 steps | Error at 1,000 |
| --- | --- | --- | --- |
| ofod (Python) | 1.72 ms | 157 ms | -6.0e-4 |
| ofod_cpp | 21 us | 1.50 ms | -6.0e-4 |
| QuantLib | 59 us | 4.29 ms | -5.9e-4 |
| FinancePy (Numba) | 12.5 us | 3.95 ms | -6.0e-4 |

FinancePy runs the same algorithm and returns ofod's price to all digits.
At 100 steps its Numba loop beats ofod_cpp through the bindings, at
1,000 steps ofod_cpp is 2.6x faster.

### American put by Crank-Nicolson finite differences

Same option, 200 price steps x 100 time steps; same reference.

| Library | Method | Time | Error |
| --- | --- | --- | --- |
| ofod (Python) | Brennan-Schwartz | 9.3 ms | -4.5e-3 |
| ofod (Python) | PSOR | 17.4 ms | -4.5e-3 |
| ofod_cpp | Brennan-Schwartz | 464 us | -4.5e-3 |
| ofod_cpp | projection | 391 us | -6.7e-3 |
| ofod_cpp | PSOR | 685 us | -4.5e-3 |
| QuantLib | projection, log grid | 1.49 ms | -2.7e-3 |
| FinancePy (Numba) | projection, log grid | 1.04 ms | -4.4e-3 |
| FinancePy (Numba) | PSOR, log grid | 19.7 ms | -4.4e-3 |

### European call by Monte Carlo

S = 50, K = 40, r = 10%, sigma = 40%, T = 0.5, 100,000 terminal draws.
Reference: the closed form, 13.0407; one standard error is 0.042.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python, NumPy) | 1.9 ms | -0.079 |
| ofod_cpp | 5.8 ms | 0.058 |
| QuantLib | 54 ms | -0.022 |
| FinancePy (Numba loop) | 3.8 ms | 0.017 |

The errors are all within two standard errors; with different random
numbers they are noise, not accuracy.

### Heston call

S = K = 100, r = 3%, T = 1, v0 = theta = 0.04, kappa = 2, sigma = 0.3,
rho = -0.7. Reference: QuantLib's adaptive Gauss-Lobatto at 1e-13.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 1.85 ms | -7e-14 |
| ofod_cpp | 598 us | -8e-14 |
| QuantLib (Gauss-Laguerre 144) | 54 us | -4e-15 |
| QuantLib (COS) | 67 us | -5e-12 |
| FinancePy (`value_lewis`, SciPy `quad`) | 1.03 ms | 6e-13 |

### Longstaff-Schwartz

American put, S = 36, K = 40, r = 6%, sigma = 20%, T = 1, 10,000 paths
and 50 exercise dates, regression on 1, S, S^2. Reference 4.4867 (CRR
American; the 50-date Bermudan is a little lower). One standard error is
0.028.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 41 ms | 0.011 |
| ofod_cpp | 26 ms | -0.029 |
| QuantLib | 597 ms | -0.052 |
| FinancePy (antithetic pairs) | 72 ms | -0.016 |

### Hull-White Bermudan swaption

Receiver swaption, flat 4% curve, a = 0.1, sigma = 1%, 10-year annual
swap at 4%, exercise on years 1 to 9, 500 tree steps (tree build
included). Reference 0.0327553: QuantLib's finite-difference engine on a
2,000 x 800 grid.

| Library | Time | Error |
| --- | --- | --- |
| ofod (Python) | 82 ms | 4.7e-5 |
| ofod_cpp | 1.25 ms | 4.7e-5 |
| QuantLib (`TreeSwaptionEngine`) | 172 ms | 2.4e-5 |

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
| ofod (Python) | 71 ms | -1.9e-7 |
| ofod_cpp | 1.82 ms | -1.9e-7 |
| FinancePy (full recursion, Numba) | 21 ms | 7e-9 |

FinancePy's 64-step rectangle rule on [-6, 6] happens to be closer than
64 Gauss-Hermite points here; both errors are below 0.002 bp.

### GARCH(1,1) fit

1,000 returns simulated from a GARCH(1,1) with omega = 2e-6, alpha = 0.1,
beta = 0.88 (seeded). The value is Hull's log likelihood,
sum of -(ln v + u^2 / v) with v_1 = u_0^2, at each library's fitted
parameters; higher is better.

| Library | Time | Log likelihood | omega | alpha | beta |
| --- | --- | --- | --- | --- | --- |
| ofod (Python) | 48 ms | 8140.511 | 2.74e-6 | 0.0995 | 0.8799 |
| ofod_cpp | 2.0 ms | 8140.511 | 2.74e-6 | 0.0995 | 0.8799 |
| arch | 4.0 ms | 8140.306 | 2.50e-6 | 0.1000 | 0.8800 |

`arch` maximises its own likelihood, which starts the variance from a
backcast rather than from u_0^2, so it scores a little lower on Hull's
definition; both recover the true parameters.

## Follow-ups

- Fix `GaussHermite` for 199 and 200 points (or cap it at 198).
- Heston: try a fixed Gauss-Laguerre rule like QuantLib's, with the
  adaptive integral kept as a check.
- C++ Monte Carlo: block normal generation (structure of arrays), which
  [Performance](PERF.md) already lists, would close the gap to NumPy and
  Numba on the European case.
- Finite differences: compare on ofod's log-space grid as well.
- Later, on the local machine: ORE for exposure and CVA, QuantLib 1.44
  from vcpkg, and desktop timings.

## Reproducing

```bash
# C++ (needs QuantLib: vcpkg's quantlib port or Ubuntu's libquantlib0-dev)
cmake -S . -B build/compare -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DOFOD_BUILD_TESTS=OFF -DOFOD_BUILD_CHAPTERS=OFF -DOFOD_BUILD_COMPARE=ON
cmake --build build/compare --target ofod_compare_quantlib
build/compare/cpp/compare/ofod_compare_quantlib --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true \
    --benchmark_out=docs/perf/compare-cpp.json --benchmark_out_format=json

# Python (ofod and ofod_cpp installed)
pip install -r tools/compare/requirements.txt
OMP_NUM_THREADS=1 NUMBA_NUM_THREADS=1 \
    python tools/compare/compare_python.py --out docs/perf/compare-python.json
```
