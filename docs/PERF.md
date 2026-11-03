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
