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
