# Hull OFOD code companion plan

Nov 2, 2026 · @Denis Zhereshchin

## Goals and scope

The companion turns all 37 chapters of Hull's *Options, Futures, and Other Derivatives* (11th ed.) into one C++20 library, Python 3 bindings, and a runnable example per chapter, built and tested on Ubuntu 24.04.

- **Three separate components.** A C++20 library (`cpp/`), an independent pure-Python library (`python/`), and a bindings component (`bindings/`) that exposes the C++ library to Python. The C++ and Python libraries never depend on each other; only the bindings depend on the C++ library.
- **Same design, two languages.** Both libraries share the module structure (`core`, `math`, `curves`, `pricers`...) and implement every chapter's topics. Python uses NumPy/SciPy and stays readable; C++ is where performance work happens.
- **One library per language, many chapters.** Each chapter adds to the shared libraries (curves, pricers, trees, Monte Carlo, finite differences) rather than shipping throwaway scripts. Chapter folders hold only examples, notebooks and exercise solutions.
- **The book is the test oracle.** Every worked example and DerivaGem number we reproduce becomes a golden test with a tolerance. All three components test against the same golden files, and the bindings tests also check C++ and Python against each other.
- **Descriptive chapters stay light.** Chapters 1, 2, 8, 10, 16 and 37 are mostly institutional; they get small simulators or calculators, not frameworks.
- **Modern C++ where it pays.** Concepts for payoffs, processes and curves; value types; `std::span` and ranges; `constexpr` math helpers; no inheritance hierarchies unless runtime polymorphism is genuinely needed (curves, vol surfaces). Python mirrors these with `typing.Protocol` and frozen dataclasses.

## Repo layout

One monorepo, `ofod11`, with three independent components: the C++ library under `cpp/`, the pure-Python library under `python/`, and the bindings under `bindings/`. Each chapter gets a thin folder under `chapters/` with a C++ example and a Python example.

```text
ofod11/
├── CMakeLists.txt            # top level: OFOD_BUILD_TESTS / _BENCH / _BINDINGS / _CHAPTERS
├── CMakePresets.json         # gcc-debug, gcc-release, clang-asan, clang-tsan, clang-tidy, coverage, profile
├── .devcontainer/            # devcontainer.json (VS Code / CLion)
├── docker/                   # Dockerfile for the ubuntu:24.04 dev image, compose.yaml
├── vcpkg.json                # Eigen, Catch2, fmt, nlohmann-json, benchmark (+ nanobind for bindings)
├── cpp/                      # COMPONENT 1: C++20 library, no Python dependency
│   ├── include/ofod/
│   │   ├── core/             # dates, day counts, compounding, strong types, errors
│   │   ├── math/             # normal CDF, bivariate normal, roots, interpolation, quadrature, optimisers
│   │   ├── random/           # RNG streams, normals, Sobol, Brownian bridge
│   │   ├── curves/           # yield, OIS, credit, vol surface; bootstrappers
│   │   ├── instruments/      # contract specs and payoffs (no pricing logic)
│   │   ├── models/           # GBM, Merton jump, Heston, Vasicek, CIR, Hull-White, BK, LMM
│   │   ├── pricers/
│   │   │   ├── analytic/     # BSM, Black, Garman-Kohlhagen, exotics closed forms
│   │   │   ├── lattice/      # binomial, trinomial, short-rate trees
│   │   │   ├── mc/           # path engine, variance reduction, Longstaff-Schwartz
│   │   │   └── fd/           # explicit, implicit, Crank-Nicolson, PSOR
│   │   ├── greeks/           # analytic + generic bump-and-reprice
│   │   ├── risk/             # VaR/ES, backtesting, PCA, scenario analysis
│   │   ├── stats/            # EWMA, GARCH(1,1), MLE, correlation
│   │   └── credit/           # hazard rates, Merton structural, CDS, Gaussian copula, CVA
│   ├── src/                  # non-template implementations -> libofod (static + shared)
│   ├── tests/                # Catch2, mirrors include/ tree; golden tests per chapter
│   └── bench/                # Google Benchmark: trees vs MC vs FD
├── python/                   # COMPONENT 2: pure-Python library, no C++ dependency
│   ├── pyproject.toml        # hatchling build, package name `ofod`
│   ├── src/ofod/             # same modules as cpp/: core, math, random, curves, ..., credit
│   └── tests/                # pytest + hypothesis; golden tests per chapter
├── bindings/                 # COMPONENT 3: C++ exposed to Python
│   ├── pyproject.toml        # scikit-build-core, package name `ofod_cpp`
│   ├── CMakeLists.txt        # links libofod, builds nanobind module ofod_cpp._core
│   ├── src/                  # one bind_<module>.cpp per C++ module
│   ├── ofod_cpp/             # .pyi stubs, small Pythonic helpers
│   └── tests/                # pytest: bindings work, and C++ == pure Python within tolerance
├── chapters/
│   ├── ch01_introduction/    # README.md, cpp/*.cpp, py/*.py, notebook.ipynb, exercises/
│   ├── ...
│   └── ch37_mishaps/
├── data/
│   ├── golden/               # book examples as JSON: inputs, expected value, tolerance, page ref
│   └── market/               # sample series for chapters 22-23 (prices, rates)
├── docs/                     # MkDocs site: one page per chapter, links to both implementations
└── tools/                    # new-chapter scaffold, golden-file checker, profile scripts
```

A `tools/new_chapter.py NN slug` scaffold creates the chapter folder, an empty golden JSON, a test stub and a docs page, so every chapter starts the same way.

## Dev environment: Docker Desktop on Windows

All development happens inside an Ubuntu 24.04 container on Docker Desktop's WSL2 backend. The same image runs locally and in CI, so "targets Ubuntu" means one pinned toolchain everywhere.

- **`docker/Dockerfile`**: `ubuntu:24.04` with GCC 13, Clang 18, clang-tidy/format, CMake, Ninja, ccache, vcpkg, gdb, Valgrind, heaptrack, `linux-tools-generic` (perf), Python 3.12 and `uv`. It's published to GHCR as `ghcr.io/dzheresc/ofod11-dev`, and CI jobs run inside it.
- **`.devcontainer/devcontainer.json`**: VS Code's "Reopen in Container" uses that image, with the C/C++, CMake Tools, Python and Jupyter extensions. CLion works too, through its Docker toolchain.
- **Source on Windows, builds in the container.** The repo is cloned to `C:\develop\ofod11`, and `C:\develop` is bind-mounted into the container at `/develop`, so the working tree is `/develop/ofod11`. Windows tools (editor, Git client, file explorer) see the same files.
- **Build output stays off the bind mount.** File access across `C:\` is slow under WSL2, so `build/`, `.venv/` and `vcpkg_installed/` are named Docker volumes mounted over those paths inside `/develop/ofod11`. Only the sources cross the Windows boundary, which keeps CMake and Python builds at near-native speed.
- **Line endings and permissions.** A `.gitattributes` with `* text=auto eol=lf` keeps shell scripts runnable in the container, and `git config core.filemode false` avoids spurious mode changes on the Windows side.
- **Named volumes for caches**: ccache, vcpkg binary cache and the uv cache, so container rebuilds don't recompile dependencies.
- **Profiling flags**: the devcontainer adds `--cap-add=SYS_PTRACE` for gdb and Valgrind, and `--cap-add=PERFMON --security-opt seccomp=unconfined` for perf and for TSan's ASLR workaround (`setarch -R`). A `profile` compose service adds `--privileged` for when a tool still refuses to run.
- **Jupyter**: port 8888 is forwarded, so notebooks open in the Windows browser.
- **Plots and KCachegrind**: matplotlib output goes to files or notebooks. KCachegrind and massif-visualizer run as Linux GUI apps through WSLg. Profile outputs written to `/develop/ofod11/build/profile` can also be copied out and opened in QCachegrind on Windows.

The compose file mounts it like this:

```yaml
services:
  dev:
    image: ghcr.io/dzheresc/ofod11-dev:latest
    volumes:
      - C:/develop:/develop
      - ofod11-build:/develop/ofod11/build
      - ofod11-venv:/develop/ofod11/.venv
      - ccache:/root/.cache/ccache
      - uv-cache:/root/.cache/uv
    working_dir: /develop/ofod11
    cap_add: [SYS_PTRACE, PERFMON]
    security_opt: [seccomp=unconfined]
    ports: ["8888:8888"]
volumes: { ofod11-build: {}, ofod11-venv: {}, ccache: {}, uv-cache: {} }
```

## Build, test and tooling

Each component builds and tests on its own. CMake presets plus vcpkg drive the C++ library. The pure-Python library is an ordinary `pyproject.toml` package with no compiled code. The bindings use scikit-build-core and nanobind to wrap the installed C++ library as a separate Python package. All three test against the same golden book examples.

| Area | Choice | Notes |
| --- | --- | --- |
| Platform | Ubuntu 24.04 LTS container (Docker Desktop, WSL2) | GCC 13 and Clang 18 from apt in the dev image; CI tests both in the same image |
| Language | C++20, Python 3.12 | `-Wall -Wextra -Wpedantic -Werror` in CI; no C++23 features so GCC 13 stays enough |
| C++ build | CMake 3.28 + Ninja + ccache | `CMakePresets.json`: `gcc-debug`, `gcc-release`, `clang-asan`, `clang-tsan`, `clang-tidy`, `coverage`, `profile`; installs `libofod` with a CMake package config so other projects (and the bindings) can `find_package(ofod)` |
| C++ deps | vcpkg manifest mode | Eigen 3.4 (linear algebra, PCA, Cholesky), fmt, nlohmann-json (golden files), Catch2 v3, Google Benchmark |
| C++ tests | Catch2 v3 + CTest | Unit tests per component, golden tests per chapter, `Approx` with explicit tolerances |
| Python library | hatchling, `uv` for envs, package `ofod` | numpy, scipy, pandas; matplotlib only in the optional `plot` extra. Pure Python, installs with `uv pip install -e python/` |
| Python tests | pytest + hypothesis | Golden tests per chapter, plus property tests: put-call parity, monotonicity in strike, tree to BSM convergence |
| Bindings | nanobind + scikit-build-core, package `ofod_cpp` | Smaller and faster to compile than pybind11; zero-copy NumPy arrays via `nb::ndarray`; `.pyi` stubs generated with `nanobind.stubgen` |
| Bindings tests | pytest | Golden tests through the bindings, plus cross-checks that `ofod_cpp` and `ofod` agree on randomised inputs (hypothesis) |
| Benchmarks | Google Benchmark, pytest-benchmark | C++ benches run on demand; pytest-benchmark compares `ofod` vs `ofod_cpp` speed per chapter |
| Notebooks | jupyterlab | Notebooks import both packages and show the two implementations side by side |
| Style | clang-format, clang-tidy, ruff, mypy (strict on `ofod`) | Enforced through `pre-commit` |
| CI | GitHub Actions on `ubuntu-24.04` | Three independent jobs, one per component (the bindings job builds the C++ library first); notebooks executed with `nbmake` weekly |
| Docs | MkDocs Material + mkdocstrings | One page per chapter linking both implementations, the notebook and the book section |

Golden tests are the backbone. Each `data/golden/chNN.json` entry stores the book reference (example or table number), inputs, expected output and tolerance. Catch2, the Python library's pytest and the bindings' pytest all read the same file, so a number is typed once and checked by all three components.

## Profiling and performance

Profiling uses three layers. Valgrind does exact, deterministic analysis on small inputs. `perf` with flame graphs samples full-size runs at real speed. Google Benchmark tracks regressions over time. Everything runs from one `profile` preset and scripts in `tools/profile/`.

| Tool | Question it answers | Where we use it |
| --- | --- | --- |
| Valgrind Memcheck | Uninitialised reads, invalid access, leaks | Weekly CI run of the full Catch2 suite via `ctest -T memcheck`; catches uninitialised grid or tree nodes that ASan misses |
| Valgrind Callgrind + KCachegrind | Exact instruction counts and call graph per function | Comparing algorithms on equal footing: CRR vs trinomial trees, explicit vs Crank-Nicolson FD, normal CDF variants, Brent vs Newton |
| Valgrind Cachegrind | Cache misses and branch mispredictions | Memory layout of trees (vector of levels vs one flat triangular buffer), FD grids (row- vs column-major), MC paths (AoS vs SoA) |
| Valgrind Massif + massif-visualizer | Peak heap and who allocated it | MC path storage, Longstaff-Schwartz regression matrices, LMM simulations, large CDO loss distributions |
| Valgrind DHAT | Short-lived and unused allocations | Hunting allocations inside hot loops: payoff evaluation, per-step vectors, `std::function` captures |
| Valgrind Helgrind / DRD | Data races and lock-order problems | The `std::jthread` MC runner and any shared accumulators; complements TSan |
| `perf record` + FlameGraph / Hotspot | Where wall time goes at real speed, with full optimisation | Full-size runs of every bench target; the main tool once code is correct |
| `perf stat` | IPC, cache-miss rates, branch-miss rates per run | Quick before/after checks on an optimisation |
| heaptrack | Allocation profile with much lower overhead than Massif | Full-size MC and credit runs, too slow under Valgrind |
| Google Benchmark + `compare.py` | Did this commit get slower? | `bench/` targets; JSON results saved per milestone and compared |
| GCC `-fopt-info-vec-missed`, Compiler Explorer | Did the inner loop vectorise? | MC step loops, FD tridiagonal solves, tree backward induction |
| TSan, ASan, UBSan | Races, memory errors and UB at near-native speed | The `clang-asan` and new `clang-tsan` presets, run in CI on every push |
| py-spy | Time spent on the Python side vs in the C++ extension | Notebooks and Python tests that call the bindings |

Build settings that make profiles readable:

- A `profile` preset: `RelWithDebInfo`, `-O2 -g -fno-omit-frame-pointer`, so perf call stacks and Valgrind source annotation both work.
- No `-march=native` in Valgrind builds. Valgrind can fail on newer instructions such as AVX-512, so we use a separate `profile-native` preset for perf only.
- Every bench binary takes `--size small|full`. Callgrind runs 20 to 100 times slower, so Valgrind gets the small size: around 10^4 MC paths, 200 tree steps and a 200 x 200 FD grid.
- Callgrind client requests (`CALLGRIND_START_INSTRUMENTATION` / `STOP`) sit behind an `OFOD_PROFILE` macro, so only the pricing kernel is measured, not setup or I/O.
- Python extension under Valgrind: run with `PYTHONMALLOC=malloc` and the CPython suppression file.

Scripts in `tools/profile/`: `callgrind.sh <bench> <filter>`, `cachegrind.sh`, `massif.sh`, `memcheck.sh`, `flame.sh <bench>` (perf record, then an SVG flame graph), and `bench_compare.sh <old.json> <new.json>`. Outputs go to `build/profile/`, which is gitignored. Each chapter that touches a numerical engine adds a short `PERF.md` with its findings: hot spot, change made, before/after numbers.

Profiling inside Docker Desktop (WSL2) changes which numbers you can trust:

- **Valgrind is unaffected.** Callgrind instruction counts and Cachegrind's simulated cache misses are deterministic, so they're the primary metric for comparing algorithms and layouts. They give the same results locally and in CI.
- **perf works, with limits.** The WSL2 kernel doesn't match Ubuntu's `linux-tools` version, so the image calls the generic perf binary directly. Software events (`cpu-clock`) and flame graphs work. Hardware counters (cache misses, IPC from `perf stat`) are often unavailable under WSL2, so we rely on Cachegrind for those.
- **Wall-clock benchmarks are noisy in a VM.** Set Windows to the high-performance power plan, give WSL2 fixed CPUs in `.wslconfig`, pin benchmarks with `taskset`, and use Google Benchmark's repetitions and median. Trust differences above about 5%.
- **If you need hardware counters later**, a native Ubuntu install or a bare-metal cloud box can run the same image unchanged.

CI runs Memcheck, the sanitizers, a smoke benchmark and a Callgrind instruction-count check on key kernels, failing on a 3% regression. Because Callgrind is deterministic, that check is reliable even on shared runners.

## Shared libraries

Thirteen modules cover every chapter; each is introduced by the first chapter that needs it and then reused. The C++ library (`libofod`, namespace `ofod::`) and the Python library (`ofod.`) have the same thirteen modules with the same names, and the bindings add one `bind_<module>.cpp` per module. Rows are in dependency order: a module only uses modules above it.

| Module | What it holds | Born in | Reused by |
| --- | --- | --- | --- |
| `core` | `Date`, day counts (Act/360, Act/365F, 30/360, Act/Act ICMA), `Compounding`, rate conversion, strong types (`Rate`, `Vol`, `YearFrac`), `CashFlow` | Ch 4 | all |
| `math` | Normal pdf/cdf (`erfc` + Hull's polynomial for ch 15), inverse cdf, bivariate normal (Drezner-Wesolowsky), Brent and Newton roots, linear/log-linear/cubic-spline interpolation, Gauss-Legendre and Gauss-Hermite quadrature, Thomas tridiagonal solver, Nelder-Mead and Levenberg-Marquardt | Ch 4, 15 | all |
| `random` | Seeded `std::mt19937_64` streams split per thread, normal generation, Cholesky-correlated normals, Sobol sequences, Brownian bridge | Ch 14 | 21-28, 33, 35 |
| `curves` | `YieldCurve` (discount, zero, forward), piecewise bootstrapper from deposits, FRAs, futures and swaps, OIS/SOFR curve, `CreditCurve` (hazard rates), `VolSurface` (strike x expiry, sticky-strike/delta) | Ch 4, 7 | 5-7, 9, 24-25, 29-34 |
| `instruments` | Plain contract descriptions: forward, future, bond, FRA, swap, option (exercise style, payoff), CDS, cap/floor, swaption. Payoffs satisfy a `Payoff` concept | Ch 5, 10 | all pricing chapters |
| `models` | Stochastic processes satisfying a `Process` concept: GBM, Merton jump-diffusion, CEV, Heston, Vasicek, CIR, Ho-Lee, Hull-White, Black-Karasinski, HJM, LMM, mean-reverting commodity | Ch 14 | 21, 27, 31-35 |
| `pricers::analytic` | BSM, Merton (dividend yield), Black-76, Garman-Kohlhagen, Bachelier, closed-form exotics (barrier, lookback, Asian geometric, compound, chooser, exchange) | Ch 15 | 17-20, 26, 29 |
| `pricers::lattice` | Generic recombining tree: CRR, Jarrow-Rudd, Tian, trinomial, control-variate tree, time-dependent parameters; short-rate trinomial tree builder | Ch 13 | 17, 18, 21, 26, 27, 32, 36 |
| `pricers::mc` | Path generator, payoff accumulator with running mean/std-err, antithetic, control variate, importance sampling, stratified, quasi-random, Longstaff-Schwartz; `std::jthread` parallel runner | Ch 21 | 22, 25-27, 33, 35 |
| `pricers::fd` | Grid + boundary conditions, explicit, fully implicit, Crank-Nicolson, PSOR for American, log-S transform | Ch 21 | 26, 27, 34 |
| `greeks` | Analytic Greeks for analytic pricers, generic central-difference bumper usable with any pricer, pathwise MC deltas | Ch 19 | 20, 22, 29 |
| `stats` + `risk` | Log returns, EWMA, GARCH(1,1) MLE, correlation/covariance, historical simulation, linear and quadratic (delta-gamma) VaR, MC VaR, ES, Kupiec backtest, PCA | Ch 22, 23 | 24, 25, 31 |
| `credit` | Hazard rate bootstrapping from bond spreads and CDS, Merton structural model, CDS pricing, one-factor Gaussian copula, Vasicek credit VaR, CVA/DVA | Ch 9, 24 | 25, 34 |

Three concepts glue them together, which keeps pricers generic without virtual dispatch:

```cpp
template <class P> concept Payoff  = requires(const P p, double s) { { p(s) } -> std::convertible_to<double>; };
template <class M> concept Process = requires(const M m, double t, double x, double dt, double z)
    { { m.step(t, x, dt, z) } -> std::convertible_to<double>; };
template <class C> concept Curve   = requires(const C c, double t) { { c.discount(t) } -> std::convertible_to<double>; };
```

Curves and vol surfaces also get a type-erased `AnyCurve` / `AnyVolSurface` wrapper so the bindings can pass them across the language boundary at runtime. On the Python side, the same three concepts are `typing.Protocol` classes (`Payoff`, `Process`, `Curve`), so `ofod` code type-checks under mypy without inheriting from anything.

## Per-chapter plan

The 37 chapters fall into seven tracks. In every row, topics are listed in the order to build them. The C++ column names what lands in `libofod` versus the chapter folder. Each of those components gets a same-named counterpart in the pure-Python `ofod` library, and a binding in `ofod_cpp`, in the same chapter. The Python column lists only what Python adds on top: plots, notebooks and data work. Every chapter ships golden tests in all three components.

### Track A: Futures, rates and swaps (Ch 1-9)

| Ch | Topics worth coding (build order) | C++ components | Python components |
| --- | --- | --- | --- |
| 1 Introduction | 1. Payoff of long/short forward and long/short call/put. 2. Simple arbitrage checker (forward vs spot and carry). 3. Hedger vs speculator P&L comparison | `instruments::Forward`, `instruments::VanillaOption` payoff functors (first use of `Payoff` concept); chapter example printing P&L tables | Payoff diagrams for the six basic positions |
| 2 Futures markets and CCPs | 1. Daily settlement and margin account simulator (initial, maintenance, margin calls). 2. Futures vs forward cash-flow timing. 3. Convergence of futures to spot | `chapters/ch02/margin_sim` using `core::Date`; no library changes beyond `core` | Reproduce the gold margin table (Table 2.1); plot margin balance over time |
| 3 Hedging using futures | 1. Minimum-variance hedge ratio and optimal contracts. 2. Basis risk simulation. 3. Beta hedging with index futures, changing portfolio beta. 4. Stack-and-roll cash flows. 5. CAPM regression (appendix) | `risk::hedge_ratio(span, span)`, `risk::beta`; uses `stats` regression helpers (first piece of `stats`) | Regression on sample data with pandas; compare to C++; hedge effectiveness plot |
| 4 Interest rates | 1. Compounding conversions (m-times, continuous). 2. Zero rates, bond pricing, bond yield (Newton). 3. Bootstrap zero curve from bonds. 4. Forward rates. 5. FRA valuation. 6. Macaulay/modified duration, convexity. 7. Par yield | Birth of `core` (dates, day counts, compounding) and `math` (roots, interpolation). `curves::YieldCurve` + `curves::BondBootstrapper`; `instruments::Bond`, `instruments::FRA`; `pricers::bond_price/yield/duration/convexity` | Plot zero and forward curves; duration-convexity approximation vs full repricing |
| 5 Forward and futures prices | 1. Forward price for investment asset, with known income and known yield. 2. Value of an existing forward. 3. Index futures, currency forwards (interest parity). 4. Commodity cost of carry with storage and convenience yield. 5. Arbitrage detector for mispriced quotes | `pricers::forward_price(spot, curve, income/yield/storage)`; `pricers::forward_value`; `instruments::Future` | Arbitrage table generator; covered interest parity check with sample FX data |
| 6 Interest rate futures | 1. Day count and Treasury price quotes (32nds). 2. Treasury bond futures: conversion factor, cheapest-to-deliver, quoted futures price. 3. Eurodollar/SOFR futures and convexity adjustment (Ho-Lee formula). 4. Extending the zero curve with futures. 5. Duration-based hedge ratio | `core::DayCount` fleshed out; `instruments::TreasuryFuture` + `pricers::conversion_factor`, `cheapest_to_deliver`; `instruments::SofrFuture` + `convexity_adjustment`; curve bootstrapper gains futures instruments | CTD analysis table; convexity adjustment vs maturity plot |
| 7 Swaps | 1. Swap cash-flow schedules (fixed, floating). 2. Bootstrap OIS/SOFR curve from swaps. 3. Value swap as bond portfolio and as FRA portfolio. 4. Value through time. 5. Fixed-for-fixed currency swap valuation (two ways). 6. Comparative-advantage calculator | `core::Schedule` (roll conventions, stubs); `curves::PiecewiseBootstrapper` generalised to deposits, futures and swaps; `instruments::InterestRateSwap`, `CurrencySwap`; `pricers::swap_npv`, `par_rate` | Bootstrap demo with plots; swap MtM path as rates move |
| 8 Securitization and the 2007-8 crisis | 1. ABS waterfall: senior, mezzanine, equity tranches. 2. ABS CDO built from mezzanine tranches. 3. Loss sensitivity to default rate | Small `chapters/ch08/waterfall` (no library change; reused as a sanity check in Ch 25) | Interactive tranche-loss vs pool-loss chart |
| 9 XVAs | 1. CVA/DVA from exposure profile and hazard rates. 2. Expected exposure of a swap by MC. 3. FVA and MVA as discounted funding spread on exposure. 4. KVA sketch. 5. Collateral and netting effects | Birth of `credit`: `credit::cva(exposure, curve, hazard)`; reuses Ch 7 swap. Full MC exposure revisited after Ch 21 and 24 | Exposure profile plots (EE, PFE); CVA sensitivity to collateral threshold |

### Track B: Options, trees and Black-Scholes-Merton (Ch 10-20)

| Ch | Topics worth coding (build order) | C++ components | Python components |
| --- | --- | --- | --- |
| 10 Mechanics of options markets | 1. Option position P&L at expiry. 2. Exchange margin requirement for naked options. 3. Stock split and dividend adjustments to strike and contract size | `instruments::VanillaOption` adds exercise style and contract adjustments; `chapters/ch10/margin` | Margin calculator examples |
| 11 Properties of stock options | 1. Upper and lower no-arbitrage bounds. 2. Put-call parity (European), bounds for American. 3. Early-exercise rules for calls/puts with dividends | `pricers::bounds` (constexpr-friendly functions); used as invariants in later tests | Hypothesis property tests: any pricer must sit inside these bounds |
| 12 Trading strategies | 1. Generic portfolio of legs with payoff and profit. 2. Spreads (bull, bear, box, butterfly, calendar, diagonal). 3. Combinations (straddle, strangle, strip, strap). 4. Principal-protected note. 5. Replicating an arbitrary piecewise-linear payoff | `instruments::Portfolio` of `(qty, Payoff)` legs; `strategies::` factory functions | Strategy gallery: one plot per strategy, reproducing the book's figures |
| 13 Binomial trees | 1. One-step no-arbitrage and risk-neutral valuation. 2. Multi-step CRR tree, European and American. 3. Delta from the tree. 4. u, d from volatility. 5. Options on index, currency, futures (via yield q). 6. Convergence to BSM (appendix) | Birth of `pricers::lattice`: `BinomialTree<Params>` templated on CRR/JR/Tian parameter policy, backward induction with an exercise policy | Convergence plot vs number of steps (odd/even oscillation); tree visualiser for small N |
| 14 Wiener processes and Ito | 1. Simulate Wiener, generalised Wiener and GBM paths. 2. Correlated processes via Cholesky. 3. Numerical check of Ito's lemma (e.g. ln S, forward price). 4. Lognormal property. 5. Fractional Brownian motion (Davies-Harte) | Birth of `random` and `models`: `models::GBM`, `models::ArithmeticBM`, `random::correlated_normals`; `models::FractionalBM` | Path plots; histogram of ln S\_T vs theoretical normal; quadratic variation demo |
| 15 Black-Scholes-Merton | 1. Normal CDF (Hull polynomial vs `erfc`). 2. BSM call/put. 3. Distribution of returns, expected return vs drift. 4. Historical volatility estimation. 5. Implied volatility (Newton with Brent fallback). 6. Known dividends (escrowed model), Black's approximation for American calls. 7. Warrant dilution | Birth of `pricers::analytic`: `bsm_price`, `implied_vol`; `stats::historical_vol`; `math::norm_cdf` | Python reference BSM with SciPy; implied vol solver comparison; vol from sample price series |
| 16 Employee stock options | 1. Binomial ESO with vesting, early-exercise multiple and exit rate (Hull-White ESO model). 2. Expected-life approach vs tree. 3. Dilution | `pricers::lattice::eso_tree` built on the Ch 13 tree with a custom exercise policy | Sensitivity of ESO value to exercise multiple |
| 17 Index and currency options | 1. Merton model with dividend yield q. 2. Garman-Kohlhagen. 3. Portfolio insurance with index puts. 4. American index/currency options on trees | `bsm_price` generalised with q; `garman_kohlhagen` as thin alias; trees already accept q | Range-forward and portfolio-insurance examples |
| 18 Futures options and Black's model | 1. Put-call parity and bounds for futures options. 2. Black-76. 3. Futures options on binomial tree (p with r = q). 4. American futures vs spot options. 5. Futures-style options | `pricers::black76`; lattice parameter policy for futures | Black vs BSM-on-forward equivalence check |
| 19 The Greek letters | 1. Analytic delta, gamma, theta, vega, rho for BSM. 2. Delta-hedging simulation with rebalancing frequency (reproduce the hedging-performance table). 3. Delta-gamma-vega neutral portfolio solver. 4. Scenario grid analysis. 5. Portfolio insurance by synthetic put. 6. ML hedging sketch (Python only) | Birth of `greeks`: `greeks::analytic<BSM>`, `greeks::bump(pricer, inputs)`; `risk::scenario_grid`; hedging simulator in `chapters/ch19` using `random` | Greek surface plots; hedge error vs rebalancing frequency; small PyTorch/sklearn hedging experiment, clearly marked optional |
| 20 Volatility smiles and surfaces | 1. Implied vol across strikes from prices. 2. Smile in delta space. 3. Vol surface interpolation (strike x expiry). 4. Minimum-variance delta. 5. Implied risk-neutral distribution via Breeden-Litzenberger (appendix). 6. Bimodal distribution for a single big jump | `curves::VolSurface` (bilinear in total variance, flat extrapolation); `analytic::implied_density` | Smile and surface 3D plots; implied density vs lognormal |

### Track C: Numerical methods and market risk (Ch 21-23)

| Ch | Topics worth coding (build order) | C++ components | Python components |
| --- | --- | --- | --- |
| 21 Basic numerical procedures | 1. Generalise binomial tree: index, currency, futures, discrete dividends (tree on S minus PV of dividends). 2. Alternative trees: equal-probability, trinomial. 3. Control-variate technique on trees. 4. Time-dependent r and sigma. 5. Monte Carlo engine with standard error. 6. Variance reduction: antithetic, control variate, importance sampling, stratified, moment matching, quasi-random (Sobol). 7. Finite differences: implicit, explicit, Crank-Nicolson, log-S grid, American via PSOR | Biggest library chapter. `pricers::lattice` gains trinomial and time-dependent params; birth of `pricers::mc` (`McEngine<Process, Payoff, Accumulator>`, parallel with `std::jthread`, reproducible per-thread streams) and `pricers::fd` (`Grid`, `ThetaScheme`, `psor`); `random::Sobol` | Convergence and error plots: tree vs MC vs FD vs analytic; variance-reduction efficiency table; Google Benchmark results charted |
| 22 VaR and expected shortfall | 1. Historical simulation VaR/ES (with volatility-scaled variant). 2. Linear model (delta-normal) for a portfolio. 3. Quadratic model with Cornish-Fisher. 4. MC VaR with full revaluation. 5. Kupiec backtest. 6. PCA on rate changes and PCA-based VaR | Birth of `risk`: `historical_var`, `es`, `linear_var`, `delta_gamma_var`, `mc_var`, `kupiec_test`; `risk::pca` on Eigen's `SelfAdjointEigenSolver` | Sample multi-asset data in `data/market`; loss distribution plots; PCA factor loadings chart |
| 23 Volatilities and correlations | 1. EWMA variance and covariance. 2. GARCH(1,1) MLE fit (reproduce the S&P 500 example). 3. Model comparison by likelihood and autocorrelation (Ljung-Box). 4. GARCH term-structure forecast. 5. EWMA/GARCH correlations, positive semi-definite check | `stats::ewma`, `stats::Garch11` with `fit()` via Nelder-Mead or L-BFGS; `stats::ljung_box`; reuses `math` optimisers | Cross-check GARCH fit against `arch` package; vol forecast plots |

### Track D: Credit (Ch 24-25)

| Ch | Topics worth coding (build order) | C++ components | Python components |
| --- | --- | --- | --- |
| 24 Credit risk | 1. Cumulative and hazard-rate default probabilities from rating tables. 2. Hazard rates from bond yield spreads. 3. Real-world vs risk-neutral comparison. 4. Merton structural model: solve for asset value and asset vol from equity. 5. Credit exposure and CVA with netting (revisit Ch 9 with MC exposure). 6. Gaussian copula default correlation. 7. Vasicek credit VaR (WCDR) and CreditMetrics-style simulation | `curves::CreditCurve` (piecewise-constant hazard); `credit::merton_solve` (2-D Newton); `credit::wcdr`, `credit::credit_var`; MC exposure via `pricers::mc` | Default-probability tables; Merton model sensitivity; credit loss distribution plots |
| 25 Credit derivatives | 1. CDS valuation: premium and protection legs, par spread, hazard rate bootstrapping from CDS quotes. 2. Fixed coupons and upfront. 3. CDS forwards and options (Black). 4. Basket nth-to-default by MC copula. 5. Synthetic CDO tranche pricing with one-factor Gaussian copula (Gauss-Hermite + recursion). 6. Implied and base correlation. 7. Alternatives: Student-t copula, random recovery | `instruments::Cds`, `pricers::cds_par_spread`, `credit::CdsBootstrapper`; `credit::GaussianCopula` with conditional-loss recursion (Andersen-Sidenius-Basu); `math::gauss_hermite` | Tranche spread vs correlation chart; base-correlation curve; reuse Ch 8 waterfall as a check |

### Track E: Exotics, advanced models and measures (Ch 26-28)

| Ch | Topics worth coding (build order) | C++ components | Python components |
| --- | --- | --- | --- |
| 26 Exotic options | 1. Packages, perpetual American options, gap, forward start, cliquet. 2. Compound and chooser (needs bivariate normal). 3. Barrier options, all eight types, closed form. 4. Binary (cash/asset-or-nothing). 5. Lookback (floating and fixed). 6. Shout (tree). 7. Asian: geometric closed form, arithmetic by moment matching and MC. 8. Exchange option (Margrabe), rainbow/basket via MC. 9. Variance and volatility swaps, VIX-style replication. 10. Static options replication of a barrier | `pricers::analytic::exotics` (one header per family); `math::bivariate_norm_cdf`; exotic `Payoff`s for the MC engine, with path-dependent payoffs taking `std::span<const double>` paths | Each closed form cross-checked by MC in tests; static-replication demo plot |
| 27 More on models and numerics | 1. CEV and Merton jump-diffusion pricing. 2. Variance-gamma. 3. Heston: semi-analytic via characteristic function, MC with full truncation. 4. Implied volatility function (local vol, Dupire) model. 5. Convertible bond on a tree with default intensity. 6. Path-dependent derivatives on trees (Hull-White representative-value method). 7. Barrier accuracy: adaptive mesh, aligning nodes. 8. Two correlated assets on a 2-D tree. 9. Longstaff-Schwartz least-squares MC for American options | `models::{CEV, MertonJump, VarianceGamma, Heston, LocalVol}`; `pricers::analytic::heston` (Gauss-Laguerre on the CF); `pricers::mc::LongstaffSchwartz` (Eigen least squares); `pricers::lattice::ConvertibleTree` | Smile produced by each model vs BSM; LSM vs tree comparison for American put |
| 28 Martingales and measures | 1. Market price of risk numerical illustration. 2. Change of numeraire demonstrations by MC: money market, zero-coupon bond (forward measure), annuity. 3. Exchange option via numeraire change, verified against Ch 26 | Mostly tests and examples: `chapters/ch28` MC experiments using existing `models` and `pricers::mc`; adds `Numeraire` policy to the MC engine | Notebook showing the same price under three measures with confidence intervals |

### Track F: Interest rate derivatives (Ch 29-34)

| Ch | Topics worth coding (build order) | C++ components | Python components |
| --- | --- | --- | --- |
| 29 Standard market models | 1. European bond options (Black on forward bond price). 2. Caps and floors as caplets via Black, cap-floor parity, flat vs spot vols, vol stripping. 3. European swaptions via Black on the swap rate and annuity. 4. Normal (Bachelier) model for low/negative rates. 5. Hedging: DV01, partial durations, bucketed vega | `instruments::{CapFloor, Swaption, BondOption}`; `pricers::analytic::{black_caplet, black_swaption, bachelier}`; `curves::CapletVolStripper`; `greeks::bucket_dv01` using curve bumps | Caplet vol term structure plot; swaption premium grid |
| 30 Convexity, timing, quanto | 1. Convexity adjustment for bond yields and swap rates. 2. Timing adjustment for payments in arrears. 3. Quanto adjustment and quanto options. 4. Siegel's paradox demo | `pricers::adjustments::{convexity, timing, quanto}`; verified by MC under forward measures from Ch 28 | Adjustment magnitude vs maturity and vol plots |
| 31 Equilibrium short-rate models | 1. Rendleman-Bartter, Vasicek and CIR bond prices (closed form). 2. Real-world vs risk-neutral simulation. 3. Parameter estimation by regression/MLE on short-rate data. 4. Two-factor sketch (Longstaff-Schwartz style) | `models::{Vasicek, CIR}` with `bond_price(t, T, r)`; exact CIR sampling (non-central chi-square); `stats` MLE reused | Yield curves implied by each model; calibration to sample rates |
| 32 No-arbitrage short-rate models | 1. Ho-Lee and Hull-White fitted to today's curve. 2. Bond options analytically (Jamshidian for coupon bonds). 3. Black-Karasinski. 4. Trinomial tree construction, the Hull-White two-stage procedure with alpha fitting. 5. General tree procedure for any f(r). 6. Calibration to caps/swaptions (Levenberg-Marquardt). 7. Hedging with the one-factor model | `models::{HoLee, HullWhite1F, BlackKarasinski}`; `pricers::lattice::ShortRateTrinomialTree` (this is the centrepiece of the track); `calibration::calibrate(model, instruments)` | Tree visualiser; Bermudan swaption pricing; calibration fit residuals |
| 33 Modeling forward rates | 1. HJM framework: drift from vol structure, MC simulation. 2. LIBOR/SOFR market model (BGM): forward-rate dynamics, drift under spot measure, caplet consistency, swaption approximation (Rebonato). 3. Volatility structure calibration. 4. Agency MBS: prepayment model, pass-through, IO/PO via MC | `models::HJM`, `models::LMM` (vectorised path step with Eigen); `pricers::mc` with multi-factor paths; `instruments::MbsPassThrough` with pluggable prepayment | LMM forward curve fan chart; MBS price vs rate (negative convexity) |
| 34 Swaps revisited | 1. Variations: amortising, step-up, forward swaps, compounding swaps. 2. Currency swaps, diff swaps, LIBOR-in-arrears, CMS/CMT swaps with convexity. 3. Equity swaps. 4. Accrual swaps, cancelable and callable swaps (Bermudan on HW tree). 5. Volatility and commodity swaps | `instruments::InterestRateSwap` generalised notional/rate schedules; reuses Ch 30 adjustments and Ch 32 trees | Payoff-to-valuation walkthroughs for each swap type |

### Track G: Commodities, real options and mishaps (Ch 35-37)

| Ch | Topics worth coding (build order) | C++ components | Python components |
| --- | --- | --- | --- |
| 35 Energy and commodity derivatives | 1. Seasonal forward curves (agricultural, gas). 2. Mean-reverting one-factor commodity model fitted to futures. 3. Trinomial tree for commodities with jumps (electricity). 4. Weather derivatives: HDD/CDD options using historical burn and simulation. 5. Cat bonds and insurance derivative pricing. 6. Producer hedging case | `models::MeanRevertingCommodity` (Schwartz 1-factor), reuse `ShortRateTrinomialTree` with a commodity drift fit; `instruments::DegreeDayOption` | Seasonal curve fit; HDD distribution from sample temperature data |
| 36 Real options | 1. NPV vs risk-neutral valuation of a project. 2. Market price of risk estimation from CAPM. 3. Valuing a business with revenue growth uncertainty (MC). 4. Options to expand, abandon, defer on a tree | `pricers::lattice` with project-value underlying and multiple exercise choices per node (`std::variant` of actions) | Decision-tree notebook; value of flexibility vs volatility |
| 37 Derivatives mishaps | 1. Small replays of lessons: leverage and margin-call cascade (Metallgesellschaft stack-and-roll), model risk (mispriced vol), rogue position limits monitor | `chapters/ch37` examples only, built from existing components | Short notebooks linking each lesson to the earlier chapter's code |

## Milestones and build order

Build mostly in book order, with two deliberate detours: the numerical engines of Ch 21 are pulled forward right after BSM, and Ch 9 (XVA) is done twice, first simply and then properly after Ch 24. Each milestone ends when its chapters' golden tests pass in CI.

1. **M0 Skeleton.** Repo, dev container image and devcontainer.json (published to GHCR), CMake presets, vcpkg, empty pure-Python package, bindings package wrapping one C++ function, pytest, CI matrix, pre-commit, golden-file loader, `new_chapter.py` scaffold. Also the profile, profile-native and clang-tsan presets, the tools/profile scripts, and a weekly Memcheck CI job. Gate: an empty chapter builds and tests green on GCC and Clang, and callgrind.sh runs on a sample benchmark.
2. **M1 Rates foundation (Ch 1, 4, 5).** `core`, `math` roots and interpolation, `YieldCurve`, bonds, FRAs, forwards. Gate: Hull's bond-bootstrap and forward-price examples match.
3. **M2 Futures and swaps (Ch 2, 3, 6, 7, 8).** Margin simulator, hedge ratios, Treasury futures and CTD, SOFR futures, schedules, multi-instrument bootstrapper, IRS and currency swaps. Gate: swap valuation examples match both ways.
4. **M3 Option basics (Ch 10-13).** Bounds, strategies, `Portfolio`, binomial tree with policies. Gate: tree examples match and converge to BSM.
5. **M4 Continuous time and BSM (Ch 14, 15, 17, 18).** `random`, `models::GBM`, `analytic` BSM/Merton/Black-76, implied vol. Gate: book option tables and implied vols reproduced.
6. **M5 Numerical engines (Ch 21).** MC engine with variance reduction, FD schemes, trinomial trees, benchmarks. First full profiling pass: Callgrind and Cachegrind on trees and FD layouts, perf flame graphs and Helgrind on the parallel MC runner, Massif on path storage. Baseline benchmark JSON saved. Gate: all three methods agree with analytic prices within stated tolerances, and PERF.md records the baseline.
7. **M6 Greeks and smiles (Ch 16, 19, 20).** `greeks`, hedging simulator, `VolSurface`, implied density, ESO tree. Gate: hedging-performance table reproduced within MC error.
8. **M7 Market risk (Ch 22, 23).** VaR/ES suite, backtests, PCA, EWMA, GARCH. Gate: GARCH estimates match the book's example data.
9. **M8 Credit (Ch 24, 25, then Ch 9 revisited).** Credit curves, Merton, CDS, Gaussian copula CDOs, MC exposure CVA. Gate: CDS and tranche examples match.
10. **M9 Exotics and advanced models (Ch 26, 27, 28).** Exotic closed forms, Heston, local vol, LSM, convertibles, numeraire experiments. Profiling pass on Heston CF integration and Longstaff-Schwartz memory (Massif, DHAT). Gate: every closed form agrees with MC, and there are no benchmark regressions against the M5 baseline.
11. **M10 Interest rate derivatives (Ch 29-34).** Black caps/swaptions, adjustments, Vasicek/CIR, Hull-White trinomial tree and calibration, HJM/LMM, MBS, exotic swaps. Profiling pass on LMM multi-factor paths (perf, Cachegrind, vectorisation reports). Gate: Hull-White tree example and Bermudan swaption run end to end.
12. **M11 Commodities, real options, mishaps (Ch 35-37).** Commodity tree, weather options, real options lattice, lesson notebooks. Gate: docs site complete with one page per chapter.

M0 to M4 is the critical path; after M5 the credit (M8) and interest-rate (M10) tracks can run in parallel because they share only `curves`, `mc` and `lattice`.

The code lives in [dzheresc/ofod11](https://github.com/dzheresc/ofod11). A milestone's gate means all three components are green: the C++ library, the pure-Python library, and the bindings cross-checks.
