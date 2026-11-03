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
| `perf.sh` | perf | Sampling profile and flame graph; needs `--cap-add SYS_ADMIN` in Docker |
| `heaptrack.sh` | heaptrack | Allocation hot spots |
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

### Postponed (needs the local machine)

- `perf.sh` sampling profiles and flame graphs (perf is not in the cloud
  container and needs `--cap-add SYS_ADMIN` in Docker) and `heaptrack.sh`.
- The vcpkg `profile` and `profile-native` (`-march=native`) presets in
  the project's Docker image; this pass used Ubuntu's packages for Google
  Benchmark, Catch2 and the rest.
- Desktop timings, to compare absolute numbers with the cloud figures.
