# Performance notes

The C++ engines are profiled with the Valgrind suite, perf and Google
Benchmark. Every profiling pass is recorded here: what was measured, what
the tools showed, and what changed. Benchmark results are saved as JSON in
`docs/perf/` so later milestones can check for regressions.

## Tools

All scripts live in `tools/profile/` and write their reports to
`build/profile-reports/`. Build the `profile` preset first (`-O2 -g
-fno-omit-frame-pointer`, benchmarks on):

```bash
cmake --preset profile && cmake --build --preset profile
```

| Script | Tool | Use it for |
| --- | --- | --- |
| `bench.sh` | Google Benchmark | Timings; saves JSON to compare with `docs/perf/baseline.json` |
| `callgrind.sh` | Callgrind | Instruction counts per function; open in KCachegrind |
| `cachegrind.sh` | Cachegrind | Cache miss rates for grid and lattice layouts |
| `massif.sh` | Massif | Heap usage over time (path storage, grids) |
| `memcheck.sh` | Memcheck | Leaks and invalid accesses (also run on every milestone) |
| `helgrind.sh` | Helgrind | Data races in the threaded Monte Carlo runner |
| `perf.sh` | perf | Sampling profile and flame graph; use the `profile` Compose service |
| `heaptrack.sh` | heaptrack | Allocation hot spots; run it on `ofod_heap_driver`, not `ofod_bench` |
| `bench_python.py` | timeit | The same calls through `ofod` (pure Python) and `ofod_cpp` |

Example:

```bash
tools/profile/callgrind.sh build/profile/cpp/bench/ofod_bench \
    --benchmark_filter=BmCrrAmerican/1024 --benchmark_min_time=1x
```

## M5 baseline (Ch 21 engines)

Measured on 4 cores at 2.1 GHz in the cloud container (GCC 13, `-O2`), so
compare ratios rather than absolute times with a desktop machine.

### Findings

1. **Binomial and trinomial trees: `std::pow` per node.** Callgrind on a
   1024-step American put showed 80% of instructions inside `libm` `pow`:
   every node recomputed `S u^j d^(i-j)`. The powers are now tabulated once
   per tree (O(N) `pow` calls instead of O(N^2)), still with `std::pow`, so
   prices are bit-for-bit unchanged.

   | Benchmark | Before | After | Speed-up |
   | --- | --- | --- | --- |
   | CRR American, 1024 steps | 14.3 ms | 1.2 ms | 11.7x |
   | CRR American, 4096 steps | 227 ms | 26 ms | 8.6x |
   | Trinomial American, 1024 steps | 16.1 ms | 2.5 ms | 6.4x |

   Both remain O(N^2), as Google Benchmark's complexity fit confirms.

2. **Finite differences: PSOR dominates.** On a 400 x 400 grid the explicit
   scheme takes 0.3 ms, while implicit and Crank-Nicolson with PSOR take
   74 ms and 44 ms. The European tridiagonal solve is cheap; the cost is the
   projected SOR iterations for the American constraint. Cachegrind shows
   D1 miss rates of 0.1% and LL miss rates of 0.0%, so the grid layout is not
   the bottleneck. A future improvement is Brennan-Schwartz (one projected
   sweep for puts) or an adaptive relaxation factor.

3. **Threaded path simulator.** 100,000 weekly Asian paths: 106 ms on one
   thread, 70 ms on two, 33 ms on four (3.2x). Results are identical for
   any thread count because each 1024-path chunk has its own seed. Helgrind
   reports no races. Massif shows a 95 KB peak heap, since paths are
   streamed per chunk rather than stored.

4. **Monte Carlo variance reduction.** All methods cost 3-6 ms for 100,000
   terminal draws; the useful comparison is standard error per unit of time
   (see `chapters/ch21_numerical_procedures`).

### Baseline timings

| Benchmark | Time |
| --- | --- |
| BSM price | 28 ns |
| Implied volatility (Newton) | 0.34 us |
| CRR American put, 64 / 256 / 1024 / 4096 steps | 7.7 us / 86 us / 1.2 ms / 26 ms |
| Trinomial American put, 64 / 256 / 1024 / 4096 steps | 17 us / 0.19 ms / 2.5 ms / 60 ms |
| FD 400 x 400, explicit / implicit / Crank-Nicolson | 0.30 ms / 74 ms / 44 ms |
| MC European, 100k paths (none ... quasi-random) | 3.2 - 5.8 ms |
| Path simulator, 100k x 52 steps, 1 / 2 / 4 threads | 106 / 70 / 33 ms |

Full results: [`perf/baseline.json`](perf/baseline.json).

## M9 pass (Ch 26-27 models)

New benchmarks: `BmHestonPrice/{1,12,60}` (maturity in months),
`BmHestonMonteCarlo` (10,000 paths x 100 steps), `BmLongstaffSchwartz/
{10000,100000}` (50 exercise dates) and `BmExoticClosedForms` (one barrier
and one compound option). Results: [`perf/m9.json`](perf/m9.json).

### Findings

1. **Heston: glibc's complex log sorts.** Callgrind on the 1-year
   semi-analytic price showed 44% of instructions in `qsort`, called from
   `__x2y2m1` inside glibc's `clog`. glibc takes an exact path for
   log|z| when |z| is close to 1, and the "little trap" term
   (1 - g e^{-dT}) / (1 - g) is always close to 1. The library now uses
   log z = (log |z|^2 / 2, arg z), which is accurate to rounding there. The
   next 11% was libgcc's `__divdc3` (complex division with overflow and
   NaN guards that cannot trigger here), replaced by a * conj(b) / |b|^2,
   and the 1/(iu) factor became Im(...)/u. Prices move by less than 1e-13.

   | Benchmark | Before | After | Speed-up |
   | --- | --- | --- | --- |
   | Heston price, 1 month | 2.06 ms | 1.07 ms | 1.9x |
   | Heston price, 1 year | 996 us | 466 us | 2.1x |
   | Heston price, 5 years | 368 us | 198 us | 1.9x |

   Short maturities stay the slowest because the characteristic function
   decays slowly in u, so more 16-point panels pass before the tail test
   stops the integral.

2. **Longstaff-Schwartz: memory is the path block, nothing else.** Massif
   on 100,000 paths x 50 dates peaks at 40.9 MB, of which 40.0 MB is the
   one contiguous block of simulated spots (paths x dates doubles, stored
   date by date so each regression reads one contiguous row). DHAT counts
   1,168 heap blocks in total for both benchmark sizes, so there are no
   allocations inside the backward induction: the regression accumulators
   are allocated once. If memory matters more than speed, the next step is
   to regenerate paths backwards with a Brownian bridge instead of storing
   them.

3. **Longstaff-Schwartz: regression accumulation.** Callgrind put 41% of
   instructions in the regression loop. The normal matrix is Hankel
   (entry (r, c) is the sum of x^(r+c)), so the loop now accumulates the
   2 deg + 1 power sums instead of all (deg + 1)^2 entries; the sums are
   identical, so prices are bit-for-bit unchanged. 100,000 paths went from
   254 ms to 193 ms. The rest is random numbers (`generate_canonical`,
   `log`, `exp`: about 40%), which is intrinsic to the method.

### Regression check against M5

Every M5 benchmark is within noise of `baseline.json` except the
single-threaded path simulator, which reads 122-126 ms against the
baseline's 106 ms. An A/B run of the same benchmark built from the M8
merge commit (`9452e80`) gives the same 123-125 ms on this container, so
the difference is the cloud host, not the code. The two- and four-thread
runs are unchanged (69 ms and 35 ms).

## M10 pass (Ch 32-33 rate models)

New benchmarks: `BmLmmMonteCarlo/{factors}/{predictor_corrector}` (20
semiannual forwards, 10,000 paths, a cap payoff) and
`BmHullWhiteBermudan/{500,2000}` (tree build plus a 10-date Bermudan
swaption over 10 years). Results: [`perf/m10.json`](perf/m10.json).
`perf` is not available in the cloud container, so the passes used
Callgrind, Cachegrind and GCC's `-fopt-info-vec` reports.

### Findings

1. **Hull-White tree: exp() in every rollback.** Callgrind on the
   2,000-step Bermudan put 47% of instructions in glibc's `exp`, called
   once per node per rollback (and again in the tree build) for the
   one-step discount factor e^{-(alpha_i + j dx) dt}. For Hull-White it
   factors into e^{-alpha_i dt} e^{-j dx dt}, so the tree now stores one
   factor per step and one per node and a rollback is multiply-adds only.
   Black-Karasinski rates are e^{alpha_i + j dx}, which do not factor, so
   it keeps the exp. Prices change by rounding only (golden cases pass at
   1e-10).

   | Benchmark | Before | After | Speed-up |
   | --- | --- | --- | --- |
   | Bermudan, 500 steps | 1.84 ms | 1.10-1.19 ms | 1.6x |
   | Bermudan, 2,000 steps | 42.1 ms | 16.6-19.8 ms | 2.1-2.5x |

2. **LMM: compute-bound, cache-resident, not vectorisable as written.**
   Callgrind on 3 factors with predictor-corrector: 38% in the drift
   accumulation, 24% in `exp` (two per forward per step with the
   corrector), 16% in the path loop and 4% in the Mersenne Twister.
   Cachegrind reports D1 and LL miss rates of 0.0%: one path is
   n(n + 1) / 2 doubles, so the working set never leaves L1. GCC's
   vectorisation report explains why there is no SIMD: the factor loops
   are 1-3 iterations, and the loop over forwards carries the running
   drift sum Sigma_i tau F_i lambda_i / (1 + tau F_i) from one forward to
   the next. Flattening the loadings into one row-major block and
   precomputing |Lambda_i|^2 / 2 left the timings within noise (about 25,
   45, 42 and 55-64 ms for 1/3 factors without/with the corrector), so
   the remaining cost is the arithmetic itself, about 13 ns per forward
   per step for one factor. The dimension that would vectorise is the
   path: simulating a block of paths in structure-of-arrays form (as the
   NumPy version does) makes every inner loop run across paths. That is
   the next step if the LMM becomes a bottleneck; it changes the payoff
   interface, so it is left for later.

3. **Predictor-corrector costs 1.3-1.8x.** It doubles the drift and exp
   work but removes most of the log-Euler drift bias; the caplet and
   bond-price tests use it.

### Regression check

M10 does not touch the Heston or Longstaff-Schwartz code. Both were
re-run alongside the new benchmarks: Heston moves by -13% to +19% against
the M9 figures, in both directions, and Longstaff-Schwartz with 100,000
paths reads 233 ms against 193 ms in M9. With unchanged code, that spread
matches the host noise already seen in the M9 A/B run. The JSON keeps
these runs for the next comparison.

## M12 pass (the rest of the library)

New benchmarks in `cpp/bench/bench_library.cc` (built into the same
`ofod_bench`): American finite differences per solver, closed-form exotics
and the variance swap strip, CDS spreads and bootstrap, CDO tranches and
implied correlation, nth-to-default baskets, exposure simulation, swap
curve bootstrap, cap bucket DV01, Black-Karasinski Bermudans, MBS Monte
Carlo, Hull-White calibration, the commodity tree, extraction projects,
the spike model, project lattices, business valuation, historical and
Monte Carlo VaR, PCA, GARCH(1,1) fits and Greeks. Results before the
changes below: [`perf/m12-before.json`](perf/m12-before.json); after:
[`perf/m12.json`](perf/m12.json). `tools/profile/bench_python.py` times
the Python package against the bindings: [`perf/python.json`](perf/python.json).

Measured on the 4-core cloud container (GCC 13, `-O2`, dependencies from
Ubuntu packages rather than vcpkg). This host is noisier than the M5-M10
ones: unchanged code such as the MBS Monte Carlo read 184 ms in one run
and 292 ms in the next, so the speed-ups below come from back-to-back
runs and are quoted as ranges where runs disagreed. Hotspots were found
with Callgrind; `perf` and heaptrack are not installed here.

### Findings

1. **American finite differences: PSOR was 90% of the time.** PSOR needs
   dozens of sweeps per time step to reach its 1e-12 tolerance, while the
   projection solver takes one Thomas solve. The new
   `AmericanSolver::kBrennanSchwartz` solves the same linear complementarity
   problem directly: for a put it eliminates the tridiagonal system
   upwards and substitutes from the bottom of the grid, projecting each
   value onto the payoff as it is found (a call runs the usual Thomas
   algorithm and projects on the way back). It is exact when the system is
   an M-matrix with one exercise boundary, which is checked at each step;
   otherwise it falls back to PSOR. `kPsor` keeps its meaning but now
   starts from the Brennan-Schwartz solution, so it needs only one or two
   confirming sweeps. Prices agree with the old PSOR to 5e-12 (the old
   PSOR's own convergence error), and a new test checks the two solvers
   against each other for puts and calls on both grids. The Python
   package and bindings gained `AmericanSolver.BRENNAN_SCHWARTZ` too.

   | Benchmark (400 x 400, American put) | Before | After | Speed-up |
   | --- | --- | --- | --- |
   | Implicit, PSOR | 91-96 ms | 7.0-8.0 ms | 12x |
   | Crank-Nicolson, PSOR | 54-56 ms | 7.0-7.8 ms | 7x |
   | Either scheme, Brennan-Schwartz | n/a | 4.4-4.9 ms | |
   | Python, Crank-Nicolson 200 x 100, PSOR | 137 ms | 19.5 ms | 7x |

2. **CDO tranches: an O(n^2) recursion for a binomial.** Callgrind put 87%
   of `TrancheSpread` in `DefaultCountDistribution`, the
   Andersen-Sidenius-Basu recursion, run for every quadrature node and
   every payment date. Every name in a `CdoPool` has the same conditional
   default probability, so the recursion is the binomial distribution;
   `ExpectedTrancheLoss` now evaluates the binomial pmf in log space (O(n)
   per node, safe at p near 0 or 1), and `TrancheLegValues` builds the
   Gauss-Hermite rule and the binomial coefficients once instead of once
   per payment date. The recursion stays public for heterogeneous pools.
   Spreads move by rounding only (the golden cases pass at 0.005 bp).

   | Benchmark | Before | After | Speed-up |
   | --- | --- | --- | --- |
   | Mezzanine spread, 125 names | 30.3 ms | 2.4-3.5 ms | 9-12x |
   | Mezzanine spread, 500 names | 264 ms | 11.6-14.2 ms | 19-23x |
   | Implied correlation, 125 names | 315 ms | 50-54 ms | 6x |

3. **Black-Karasinski tree: exp() everywhere.** 75% of instructions were
   in `exp`: the alpha fit called Brent on [-30, 3], with two exps per node
   per trial, and every forward-induction and rollback discount computed
   exp(-e^{alpha + j dx} dt) again. The tree now tabulates e^{j dx}, so a
   trial alpha costs one exp per node; solves for alpha by Newton-Raphson
   from the previous step's alpha (Hull's suggestion; value and slope share
   each exp, three iterations instead of dozens) with Brent as the
   fallback; and stores each step's node discounts once, so rollbacks call
   no exp at all. Newton stops at Brent's 1e-12 tolerance, because
   rounding in the sum limits alpha to about 1e-13. Golden swaption and
   bond-option cases still pass at 1e-10.

   | Benchmark | Before | After | Speed-up |
   | --- | --- | --- | --- |
   | Bermudan swaption, 500 steps | 28.9 ms | 4.9-5.1 ms | 5.8x |
   | Bermudan swaption, 2,000 steps | 476 ms | 126-134 ms | 3.6x |

4. **Real-option lattice: the M5 `pow` bug again.** `ProjectWithOptions`
   and `DeferralOption` computed V u^k d^(i-k) with two `std::pow` calls per
   node. The powers are now tabulated once per lattice, still with
   `std::pow`, so values are bit-for-bit unchanged.

   | Benchmark | Before | After | Speed-up |
   | --- | --- | --- | --- |
   | Abandon/expand/contract, 500 steps | 6.2 ms | 1.1-1.4 ms | 4.4-5.5x |
   | Abandon/expand/contract, 2,000 steps | 98 ms | 23-30 ms | 3.2-4.3x |

5. **Commodity tree: exp per node in `Price`.** The American check and
   the extraction project called exp(alpha_i + j dx) at every node, and
   the build recomputed e^{j dx} at every step. Both factors are now
   tabulated (as for Hull-White in M10); prices change by rounding only and
   the golden cases pass at 1e-12. American put, 500 steps: 2.5 ms to
   1.2 ms; 2,000 steps: 43 ms to 20 ms (2.1x).

6. **Historical VaR sorted every scenario.** Only the k = n (1 - X) worst
   losses matter, so `HistoricalVar` and `HistoricalExpectedShortfall` use
   a partial sort (same values, same order). 100,000 scenarios at 99%:
   15.6 ms to 1.2-1.3 ms; 500 scenarios: 8.9 us to 1.6 us.

7. **Monte Carlo VaR: the Cholesky product.** 63% of instructions with 50
   factors were in `random::Correlate`, which accumulated into `out[i]` and
   so stored and reloaded it on every term. A local accumulator with the
   same summation order (bit-for-bit) cut 50 factors x 100,000 paths from
   563 ms to 305-371 ms. The remainder is the normal draws.

### Left as they are

- **Random-number-bound Monte Carlo.** The nth-to-default basket, spike
  model, business valuation, exposure simulation and MBS engines spend
  25-40% of their instructions in `std::mt19937_64` with
  `generate_canonical` and `std::normal_distribution`, and much of the
  rest in `log`, `exp` and `erfc`. Faster generators would change every
  seeded result in the golden files, so this is noted rather than done.
  The MBS engine also spends 18% in `pow` for the monthly mortality
  1 - (1 - CPR)^(1/12), which varies by path.
- **GARCH(1,1) fit:** 37% in `log`, intrinsic to the likelihood.
- **Closed forms, CDS, curves, caps, calibration and Greeks** are already
  in the microsecond range (0.5 us for six exotic formulas, 3 us for a CDS
  par spread, 0.45 ms for a 30-year swap bootstrap, 0.1 us for analytic
  Greeks against 4.6 ms for bumping a 500-step American tree nine times).

### C++ against Python

`tools/profile/bench_python.py`, best of three, the same inputs through
both packages (the bindings add the call overhead):

| Case | Python | C++ (bindings) | Ratio |
| --- | --- | --- | --- |
| BSM price | 1.04 us | 168 ns | 6x |
| Implied volatility | 9.95 us | 720 ns | 14x |
| CRR American, 1,000 steps | 254 ms | 2.08 ms | 122x |
| Trinomial American, 500 steps | 99.8 ms | 0.91 ms | 110x |
| FD Crank-Nicolson American, PSOR | 19.5 ms | 0.92 ms | 21x |
| MC European, 100,000 paths | 4.65 ms | 6.55 ms | 0.7x |
| Heston price, 1 year | 3.08 ms | 0.71 ms | 4x |
| Longstaff-Schwartz, 10,000 paths | 62 ms | 33 ms | 1.9x |
| Hull-White Bermudan, 500 steps | 126 ms | 1.85 ms | 68x |
| Tranche spread, 125 names | 113 ms | 2.63 ms | 43x |
| GARCH(1,1) fit, 1,000 returns | 156 ms | 4.39 ms | 35x |
| Project lattice, 500 steps | 15.9 ms | 1.02 ms | 16x |

Scalar loops (trees, PDE sweeps, likelihoods) are 20-120x faster in C++.
Where the Python code is vectorised with NumPy the gap closes: the
Longstaff-Schwartz regression is 1.9x, and the European Monte Carlo is
faster in NumPy, whose vectorised normal generator beats one
`std::normal_distribution` draw at a time. That is the same conclusion as
the M10 LMM note: simulating blocks of paths in structure-of-arrays form
is the next step for the C++ Monte Carlo engines.

### Regression check

Every M5-M10 benchmark was re-run with the new code (in `m12.json`). The
code they exercise is unchanged except the shared `Correlate`, and they
sit within the host noise described above.

The items postponed here for the local machine are in the
[desktop pass](#desktop-pass).

## Comparison follow-up: closing the gaps

[Comparison](COMPARISON.md) found three cases where ofod was slower than
another library: the Heston price (12x slower than QuantLib), the 100,000
path European Monte Carlo (slower than FinancePy's Numba loop and than
ofod's own NumPy code) and, through the bindings, the 100-step CRR tree
(slower than FinancePy). This pass works on those three only. Before and
after, same container, `profile` build, median of 5:
[`perf/m13-before.json`](perf/m13-before.json) and
[`perf/m13.json`](perf/m13.json).

| Benchmark | Before | After | Speed-up |
| --- | --- | --- | --- |
| Heston price, 1 month | 1.35 ms | 29 us | 46x |
| Heston price, 1 year | 667 us | 29 us | 23x |
| Heston price, 5 years | 244 us | 31 us | 8x |
| MC European, 100k paths, default generator | 4.93 ms | 4.51 ms | 1.1x |
| MC European, 100k paths, `Rng::kZiggurat` | | 1.70 ms | 2.9x |
| One normal draw, default / ziggurat | 24.5 ns | 2.9 ns | 8x |
| CRR American put, 64 steps | 9.4 us | 5.2 us | 1.8x |
| CRR American put, 1,024 steps | 1.48 ms | 0.86 ms | 1.7x |
| CRR American put, 4,096 steps | 25.8 ms | 16.7 ms | 1.5x |

### Findings

1. **Heston: too many integrand evaluations, not slow ones.** One
   characteristic function costs about 130 ns (Callgrind: `atan2`, `cos`,
   `exp`, `csqrt` and complex arithmetic in roughly equal parts), the same
   as in QuantLib. The difference was the count: two characteristic
   functions per node (Hull's form) on 16-point panels of width 1, 3,552
   evaluations for one year against QuantLib's 144-node rule. Three
   changes bring it to about 220:
   - **Lewis's single integral.** One characteristic function per node,
     evaluated at u - i/2, with the factor e^{iux} folded into the cosine
     of the final exponential.
   - **A Black-Scholes control variate** at the expected average variance.
     Subtracting the Black-Scholes integrand (known in closed form) makes
     the remainder small near zero, and it removes the poles of
     1 / (u^2 + 1/4) at +-i/2, because both characteristic functions
     equal 1 there. Without it, 16-point panels wider than about 1 lose
     accuracy near the origin (errors of 1e-5 at width 4).
   - **Panels sized to the integrand.** The width comes from the
     integrand's length scales: e^{-w u^2 / 2} near zero, the tail decay
     sqrt(1 - rho^2) (v0 + kappa theta T) / sigma and phase rate, and the
     moneyness. Panels start at 1/2 and double up to that width, because
     the characteristic function has poles on the imaginary axis at
     distance 1/2 or more (moment explosions); doubling keeps each panel
     as far from them, relative to its width, as the first one. The
     stopping test is unchanged (three panels below 1e-14 (1 + |I|)).

   QuantLib's fixed 144-point Gauss-Laguerre rule was the suggested
   model, but a fixed rule has no error check, and short maturities need
   integrals out to u in the thousands. The adaptive panels keep the
   check and still beat it: 28 us against 42 us in
   [Comparison](COMPARISON.md). Accuracy was checked on 588 cases (12
   parameter sets including sigma = 2 with rho = -0.95, kappa = 0 and
   v0 = 0; maturities from one week to 30 years; strikes 50-200) against
   two independent integrals with fine fixed panels (Hull's form and
   Lewis's without the control variate). Where the references are
   converged, the new prices agree with them to about 1e-11, the
   references' own rounding; where they disagree with each other (very
   short maturities, slow tails), the new price sits with the one that is
   stable under panel refinement. A new unit test checks 30 cases at
   1e-10 against a reference written separately in the test.

   The old integral had a real failure: with kappa = 0, rho = 0.9 and
   T = 30, Hull's form gave calls of 45.6 at K = 50, below the
   no-arbitrage bound S e^{-qT} - K e^{-rT} = 53.8; the new form gives
   55.7, which the reference confirms. The two golden Heston cases (1 year)
   move by 7e-14 and 3e-13. The Python package uses the same method and
   went from 1.85 ms to 0.44 ms.

   Smaller items: the Gauss-Legendre rule is built once (static), and
   `std::sqrt` on complex numbers (glibc `csqrt`, which goes through
   `hypot`) is replaced by the textbook formula.

2. **Monte Carlo: the normal generator, as expected.** Callgrind on the
   default European engine put 33% of instructions in
   `std::normal_distribution` (the Marsaglia polar method: two uniforms, a
   logarithm, a square root and a division per pair, with 21% of pairs
   rejected), 9% in `log`, 5% in the Mersenne Twister, 22% in `exp` for
   S_T and 6% in `memset` for two zero-filled 800 KB vectors. NumPy's
   `standard_normal` is a ziggurat, which is why the vectorised Python
   was faster than the C++.

   Any faster generator changes the random numbers and so every seeded
   result, so the new one is opt-in: `McOptions::rng = Rng::kZiggurat`
   (`Rng.ZIGGURAT` in the bindings) uses xoshiro256++ and a 256-layer
   Marsaglia-Tsang ziggurat. About 99% of draws cost one 64-bit integer,
   a table lookup and a multiply; the layer index and sign come from the
   low 9 bits and the uniform from the top 53, so they are independent
   (the flaw Doornik pointed out in the original). The sign is applied
   by flipping the sign bit; a branch on that coin flip mispredicted half
   the time and cost more than the rest of the draw (7 ns against 3 ns).
   Tests check the xoshiro256++ stream against values computed
   independently, the first four moments, the frequency of tail draws
   beyond R, a Kolmogorov-Smirnov test on 10^6 draws, and every variance
   reduction method against Black-Scholes.

   The default path also stopped zero-filling vectors it then overwrote
   and calls an inlined payoff; prices and standard errors are
   bit-for-bit identical to before for every method (checked against a
   build of the previous commit). The remaining 17 ns per path with the
   ziggurat is mostly `exp` for S_T; vectorising it would need
   `-ffast-math` or a vector math library.

   Stratified sampling and quasi-random draws use the inverse normal
   rather than the generator, so they do not change. The path simulator
   and the other Monte Carlo engines still use the default generator
   only; offering them the option is a small change if it is ever
   needed.

3. **CRR tree: the payoff was a function call per node.** The American
   check called `IntrinsicValue`, which lives in another translation
   unit, once per node, so the rollback loop could not be inlined or
   vectorised. An inlined max(phi (S - K), 0), which gives the same
   values (K - S is exactly -(S - K)), makes the loop vectorise: 1.5-1.8x
   in C++, and through the bindings the 100-step tree went from 21 us to
   6.9 us against FinancePy's 13 us. Prices and deltas are bit-for-bit
   unchanged for all three schemes, both exercise styles and both option
   types (checked against the previous commit).

Desktop timings of the three changes and `perf` profiles of the ziggurat
loop are in the [desktop pass](#desktop-pass).

## Desktop pass

The work the cloud passes left for the local machine: the vcpkg builds in
the project's Docker image, `perf` and heaptrack, and timings on a quieter
host. It includes the gap-closing changes above. Results:
[`perf/desktop.json`](perf/desktop.json) (`profile`),
[`perf/desktop-native.json`](perf/desktop-native.json) (`profile-native`),
[`perf/desktop-threads.json`](perf/desktop-threads.json) and
[`perf/python-desktop.json`](perf/python-desktop.json). Flame graphs are in
`docs/perf/flame/`.

| | |
| --- | --- |
| Host | Intel Core Ultra 9 185H laptop on mains power, Windows 11, Balanced plan |
| Container | Ubuntu 24.04 in Docker Desktop on WSL2 (kernel 5.15), 22 vCPUs, 16 GB |
| Build | GCC 13.3, dependencies from vcpkg, `-O2 -g -fno-omit-frame-pointer` |
| Method | Median of 5, pinned to CPU 4; threaded runs on CPUs 0, 2, 4 and 6 |

The 185H mixes performance and efficiency cores, but WSL2 shows 11 equal
cores with two threads each, and Windows decides where each vCPU runs, so
pinning does not choose a physical core. Even-numbered CPUs avoid sharing
a core between threads as far as the guest can tell. A 1,024-step CRR
benchmark read 0.58-0.63 ms on all 16 vCPUs tried in a second pass, so
single-threaded results do not depend on the CPU chosen. The 217 C++ tests
and 777 Python tests pass in this build.

### Timings

The median benchmark is 1.8x faster than the same benchmark in the cloud
JSON, and most sit between 1.3x and 2.8x. Part of that spread is cloud
noise, so these are the figures to use as a baseline from now on.

| Benchmark | Cloud | Desktop | Speed-up |
| --- | --- | --- | --- |
| BSM price | 47 ns | 26 ns | 1.8x |
| CRR American put, 1,024 steps | 0.86 ms | 0.57 ms | 1.5x |
| CRR American put, 4,096 steps | 16.7 ms | 11.2 ms | 1.5x |
| Heston price, 1 year | 29 us | 19 us | 1.5x |
| MC European, 100k paths, default generator | 4.51 ms | 3.20 ms | 1.4x |
| MC European, 100k paths, `Rng::kZiggurat` | 1.70 ms | 1.43 ms | 1.2x |
| One normal draw, default / ziggurat | 25 / 2.9 ns | 18 / 2.8 ns | 1.4x / 1.0x |
| Longstaff-Schwartz, 100,000 paths | 424 ms | 194 ms | 2.2x |
| FD American, Crank-Nicolson 400 x 400, PSOR | 7.5 ms | 3.5 ms | 2.1x |
| Tranche spread, 125 names | 3.5 ms | 1.4 ms | 2.5x |
| Hull-White Bermudan, 2,000 steps | 35.5 ms | 23.9 ms | 1.5x |
| MBS Monte Carlo | 292 ms | 91 ms | 3.2x |

The ziggurat's lead over the default generator holds: 2.2x on the
European engine and 6x per draw.

**Threads.** The path simulator takes 116, 69 and 38 ms on one, two and
four threads, so four threads give 3.1x, in line with the cloud's 3.2x.
The rows for two and four threads are in `desktop-threads.json` only:
the pinned suite ran them on one CPU, so they were dropped from the other
two files.

### `-march=native`: use `x86-64-v3` instead

The median benchmark does not move (1.02x), but some do. A/B runs on this
host, three rounds each, gave:

| Benchmark | `profile` | `-march=native` | `native -mtune=generic` | `-march=x86-64-v3` |
| --- | --- | --- | --- | --- |
| MC European, ziggurat | 1.43 ms | 1.06 ms | 0.97 ms | 0.96 ms |
| One ziggurat draw | 2.8 ns | 5.5 ns | 2.8 ns | 2.7 ns |
| CRR American put, 1,024 steps | 0.59 ms | 0.64 ms | 0.63 ms | 0.62 ms |
| Implied tranche correlation | 22.8 ms | 26.9 ms | 26.1 ms | 25.8 ms |

GCC 13 resolves `native` on this CPU to `-march=alderlake -mtune=alderlake`,
and that tuning doubles the cost of a ziggurat draw. Generic tuning
removes the penalty, so the cause is the tuning, not the instruction set.
`-march=x86-64-v3` (AVX2 and FMA) keeps the Monte Carlo gain of 1.5x, but
CRR and the tranche correlation stay 5-13% slower than plain x86-64 under
every AVX2 build. The mechanism was not traced further, because hardware
counters are unavailable (see below), and the `profile-native` preset is
unchanged. In the full suite, `native` also speeds up the project lattice
(1.4x) and PCA (1.2x).

### Profiles

WSL2's kernel exposes no hardware counters: `perf stat` reports cycles and
instructions as not supported, so these profiles sample on the CPU clock
(`task-clock`), and cache and branch questions still need Cachegrind.
Self time, from `perf report --no-children`:

| Benchmark | Where the time goes |
| --- | --- |
| CRR American, 1,024 steps | 96% rollback loop in `PriceTree`, 3% `pow` for the tables |
| MC European, ziggurat | 46% engine loop with the inlined draw, 45% `exp` with its wrapper and PLT stubs, 7% `SampleMoments` |
| Heston, 1 year | 32% integrand, 27% `cos` and `sincos`, 14% `log`, 13% `atan2`, 10% `exp` and `cexp` |
| Longstaff-Schwartz, 100,000 paths | 55% engine, 17% `exp`, 14% Mersenne Twister and `generate_canonical`, 10% `log` |
| Forward exposure | 35% `std::sort`, 16% `ProfileFromPaths`, 13% path loop, the rest random numbers and `exp` |
| Hull-White Bermudan, 2,000 steps | 44% tree `Build`, 24% `Rollback`, 21% `Discount`, 9% `BranchAt` |
| MBS Monte Carlo | 35% engine, 26% `expm1`, 13% `pow`, 7% `generate_canonical` |
| Monte Carlo VaR, 50 factors | 33% `Correlate`, 27% engine, 20% Mersenne Twister and `generate_canonical`, 15% `log` |

These agree with the Callgrind findings above, with three additions:

1. **Forward exposure spends a third of its time sorting.** The sort
   under `ProfileFromPaths` is there to read one quantile per date, so
   `std::nth_element` should remove most of that 35%. The paths are also
   held as one vector per path, which shows up as about 10,000 allocations
   per call (see below).
2. **Hull-White: `Discount` and `BranchAt` are not inlined.** They take
   30% of the Bermudan as separate functions. Moving them into the header,
   or enabling LTO, is the obvious next try.
3. **The ziggurat loop is now `exp`-bound.** The draw no longer stands
   out: the loop spends about as much in `exp` for S_T as in everything
   else. That supports the note above that only a vector `exp` would help.

None of these were changed in this pass, which only measures.

### Allocations

heaptrack stops recording as soon as `ofod_bench` starts (it reports the
same 393 startup allocations for every benchmark; the cause was not
found), so `ofod_heap_driver` runs each hot path 20 times without Google
Benchmark. No case leaks.

| Case | Allocations per call | Peak heap |
| --- | --- | --- |
| CRR American, 1,024 steps | 3 | 99 KB |
| MC European, ziggurat, 100k paths | 1 | 874 KB |
| Longstaff-Schwartz, 100,000 paths | 303 | 40.9 MB |
| Forward exposure, 10,000 paths x 60 dates | 10,092 | 5.3 MB |

Longstaff-Schwartz's 40.9 MB matches Massif's figure from M9. Peak heap
includes about 74 KB of `libstdc++` startup allocation.

### C++ against Python

Best of three, through `tools/profile/bench_python.py`.

| Case | Python | C++ (bindings) | Ratio |
| --- | --- | --- | --- |
| BSM price | 738 ns | 138 ns | 5x |
| Implied volatility | 7.84 us | 383 ns | 20x |
| CRR American, 100 steps | 2.88 ms | 4.75 us | 607x |
| CRR American, 1,000 steps | 299 ms | 285 us | 1,048x |
| Trinomial American, 500 steps | 127 ms | 444 us | 285x |
| FD Crank-Nicolson American, PSOR | 25.4 ms | 445 us | 57x |
| FD Crank-Nicolson American, Brennan-Schwartz | 16.5 ms | 309 us | 53x |
| MC European, 100,000 paths | 2.34 ms | 3.48 ms | 0.7x |
| Heston price, 1 year | 362 us | 19.5 us | 19x |
| Longstaff-Schwartz, 10,000 paths | 35.4 ms | 18.7 ms | 1.9x |
| Hull-White Bermudan, 500 steps | 110 ms | 1.26 ms | 87x |
| Tranche spread, 125 names | 68.3 ms | 1.42 ms | 48x |
| GARCH(1,1) fit, 1,000 returns | 146 ms | 2.42 ms | 60x |
| Project lattice, 500 steps | 7.41 ms | 396 us | 19x |

The ratios are larger than in M12 mostly because the C++ has improved
since then. The NumPy cases are unchanged: Monte Carlo stays at 0.7x with
the default generator and Longstaff-Schwartz at 1.9x.

Pure-Python loops ran slower here than in the cloud (the 1,000-step tree
took about 300 ms against the cloud's 170 ms). The container uses Ubuntu's
Python 3.12.3, and the cloud used 3.13. On this host Python 3.13.9 takes
227 ms for that tree, so the version explains most of the gap, though not
all of it.
