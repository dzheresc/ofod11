# Comparison with other libraries

This page picks the open-source libraries to benchmark `ofod` against, and
the functions to compare. Tier 1 has been run in the cloud container: the
results are in [Comparison results](COMPARISON.md), and our own timings
are in [Performance](PERF.md).

The aim of each comparison is two numbers on the same inputs and the same
host: the time per call, and the difference in price from a reference
value. A faster engine that is less accurate, or a slower one that is more
accurate, is not a like-for-like result, so both are always reported.

## Candidates

Versions are the latest on PyPI in November 2026.

| Library | Language | Licence | Status | Overlap with `ofod` |
| --- | --- | --- | --- | --- |
| [QuantLib](https://www.quantlib.org) 1.44 | C++ (SWIG Python wheel `QuantLib`) | BSD-3 | Active, quarterly releases | Very wide: every chapter except MBS, real options, ESOs, weather and most of Ch 22-23 |
| [FinancePy](https://github.com/domokane/FinancePy) 1.1.2 | Python with Numba | GPL-3.0 | Active | Wide, and organised close to Hull: equity exotics, Heston/SABR, HW/BK/BDT trees, LMM, CDS, CDO tranches |
| [ORE](https://github.com/OpenSourceRisk/Engine) 1.8.17 | C++ on QuantLib (Python `open-source-risk-engine`) | Modified BSD | Active (LSEG) | Exposure simulation, CVA/DVA/FVA, initial margin |
| [arch](https://github.com/bashtage/arch) 8.0 | Python (Cython/Numba) | NCSA | Active | GARCH fitting and volatility forecasts (Ch 23) |
| [vollib](https://github.com/vollib/vollib) 1.0.11 | Python (Let's Be Rational) | MIT | Maintained, thin | Black and BSM prices, implied volatility, analytic Greeks |
| [PyFENG](https://github.com/PyFE/PyFENG) 0.5 | Python/NumPy | GPL-2.0 | Active, research code | Heston, SABR, stochastic volatility Monte Carlo |
| [rateslib](https://github.com/attack68/rateslib) 2.7 | Python with Rust | Source-available, non-commercial | Active | Curve building, swaps, bonds, FX |
| [TF Quant Finance](https://github.com/google/tf-quant-finance) | Python on TensorFlow | Apache-2.0 | Archived, unmaintained | Wide (PDE, MC, LMM, HJM), batched on GPU |

## Shortlist

1. **QuantLib, in C++ and through its Python wheel.** The industry
   reference, it covers most of the book, and it is the only candidate
   that gives a C++ against C++ comparison with Google Benchmark. vcpkg
   has a `quantlib` port, so it fits the existing build as an optional
   feature. Through the `QuantLib` wheel it also gives a fair comparison
   for `ofod_cpp`, because both are C++ behind a Python binding.
2. **FinancePy.** The closest Python library in scope (its author follows
   Hull's chapters closely), and Numba-compiled, so it tests the pure
   Python `ofod` against a different speed-up strategy than our C++
   bindings. Numba compiles on the first call, so it needs a warm-up call
   before timing.
3. **arch,** for the one Ch 23 function neither of the above covers well:
   the GARCH(1,1) maximum-likelihood fit.
4. **vollib,** for implied volatility. Let's Be Rational converges in two
   iterations to machine precision, so it is the accuracy and speed
   reference for our Newton solver. Jaeckel's C++ reference code can be
   added to the C++ side for the same comparison.

**Later, optional: ORE,** for exposure simulation and CVA (Ch 9). It is
the only real comparator there, but it is configured through XML
portfolios and market files and builds with Boost and QuantLib, so it
should wait for the local Docker machine.

**Left out:**

- **TF Quant Finance** is archived and needs TensorFlow; its batched GPU
  design measures something different from our single-threaded engines.
- **rateslib** is source-available rather than open source, and only
  overlaps with our curve bootstrap, which QuantLib already covers.
- **PyFENG** overlaps with QuantLib and FinancePy for Heston; worth a look
  only if the Heston comparison raises questions.

## Functions to compare

The cases reuse the inputs of our existing benchmarks (`cpp/bench/` and
`tools/profile/bench_python.py`), so each comparison has a matching row in
[Performance](PERF.md). Settings that change accuracy (steps, grid size,
paths, tolerances, integration rule) are set the same on both sides, and
the table names the reference value for the error column.

QL is QuantLib, FP is FinancePy.

### Tier 1: run first

| Our case | Chapter | QuantLib | FinancePy / other | Reference for error |
| --- | --- | --- | --- | --- |
| `BmBsm`: BSM price | 15 | `BlackCalculator`, `AnalyticEuropeanEngine` | FP `EquityVanillaOption`; vollib `black_scholes` | Closed form |
| `BmImpliedVol` | 20 | `blackFormulaImpliedStdDev` (and `VanillaOption::impliedVolatility`) | vollib `implied_volatility`; FP `implied_volatility` | Input volatility |
| `BmBsmGreeks` | 19 | `AnalyticEuropeanEngine` Greeks | FP `EquityVanillaOption` delta/gamma/vega/theta | Closed form |
| `BmCrrAmerican/{64..4096}` | 13, 21 | `BinomialVanillaEngine<CoxRossRubinstein>` | FP `EquityAmericanOption` (tree) | 10,000-step tree |
| `BmTrinomialAmerican` | 21 | `BinomialVanillaEngine<Trinomial>` | - | 10,000-step tree |
| `BmFdAmericanSolver`, `BmFiniteDifference` | 21 | `FdBlackScholesVanillaEngine` (Douglas, Crank-Nicolson schemes) | FP `black_scholes_fd`, `black_scholes_fd_psor` | Fine-grid FD |
| `BmMonteCarloEuropean` | 21 | `MCEuropeanEngine` (pseudo and Sobol) | FP `EquityVanillaOption` Monte Carlo | Closed form; compare error per second |
| `BmLongstaffSchwartz` | 27 | `MCAmericanEngine` | FP `equity_lsmc` | Fine tree |
| `BmHestonPrice/{1,12,60}` | 27 | `AnalyticHestonEngine` (Gauss-Laguerre and adaptive Gauss-Lobatto) | FP `Heston` (`models/heston.py`); PyFENG if needed | QL adaptive at 1e-12 |
| `BmHullWhiteBermudan` | 31, 32 | `TreeSwaptionEngine` with `HullWhite` | FP `IborBermudanSwaption` with `HWTree` | Same model, 10,000 steps |
| `BmTrancheSpread` | 25 | `IntegralCDOEngine` with `GaussianLHPLossModel` or `HomogeneousPoolLossModel` | FP `CDSTranche` (recursion and LHP methods) | Our recursion, finest quadrature |
| `BmFitGarch` | 23 | (`Garch11` exists but is not a full MLE) | arch `arch_model(...).fit()` | Log-likelihood at the optimum |

### Tier 2: after Tier 1

| Our case | Chapter | QuantLib | FinancePy / other |
| --- | --- | --- | --- |
| `BmExoticClosedForms`, `BmExoticFormulas` | 26 | `AnalyticBarrierEngine`, `AnalyticContinuousGeometricAveragePriceAsianEngine`, `AnalyticCompoundOptionEngine`, lookback and chooser engines | FP `EquityBarrierOption`, `EquityAsianOption`, `EquityCompoundOption`, `EquityFixedLookbackOption`, `EquityFloatLookbackOption`, `EquityChooserOption` |
| `BmVarianceSwap` | 26 | `ReplicatingVarianceSwapEngine` | FP `EquityVarianceSwap` |
| `BmCdsParSpread`, `BmBootstrapCreditCurve` | 25 | `MidPointCdsEngine`, `IsdaCdsEngine`, `PiecewiseDefaultCurve` | FP `CDS`, `CDSCurve` |
| `BmImpliedTrancheCorrelation` | 25 | Root search around `IntegralCDOEngine` | FP `CDSTranche` with a root search |
| `BmNthToDefault` | 25 | `IntegralNtdEngine` (experimental) | FP `CDSBasket` |
| `BmBootstrapSwaps` | 4, 7 | `PiecewiseYieldCurve` with `SwapRateHelper` | FP `IborSingleCurve` |
| `BmCapBucketDv` | 29 | `BlackCapFloorEngine` with bumped curves | FP `IborCapFloor` |
| `BmCalibrateHullWhite` | 32 | `HullWhite::calibrate` with `SwaptionHelper`, Levenberg-Marquardt | - |
| `BmBlackKarasinskiBermudan` | 32 | `TreeSwaptionEngine` with `BlackKarasinski` | FP `IborBermudanSwaption` with `BKTree` |
| `BmLmmMonteCarlo` | 33 | `LiborForwardModel` with `MarketModel` evolvers (`LogNormalFwdRatePc`) | FP `models/lmm_mc.py` (`lmm_simulate_fwds_*`) |
| `BmHestonMonteCarlo` | 27 | `MCEuropeanHestonEngine` (QE discretisation) | FP `Heston` Monte Carlo |
| `BmSpikeMonteCarlo` | 35 | `ExtOUWithJumpsProcess` (Kluge model) path generation | - |
| `BmForwardExposure` | 9 | - | ORE exposure simulation |
| `BmBumpGreeksTree` | 19 | Bumped `BinomialVanillaEngine` | FP tree Greeks |

### Not compared

No candidate implements Hull's versions of these, so there is nothing
like-for-like to time: the MBS prepayment Monte Carlo (Ch 8), employee
stock options (Ch 16), the commodity trinomial tree and extraction
projects (Ch 35), real-option lattices and business valuation (Ch 36),
and weather insurance. Historical VaR, Monte Carlo VaR and PCA (Ch 22) are
a few lines of NumPy in any library, so they are compared with NumPy
itself only if a question comes up.

## Method

- **Same host, same settings, one thread.** Each comparison runs both
  sides back to back on one machine (the cloud container is noisy, see
  [Performance](PERF.md)); QuantLib and Numba thread pools are pinned to
  one thread. Timings are best of five, as in `bench_python.py`.
- **C++ against C++:** QuantLib cases go in a separate Google Benchmark
  binary (`cpp/compare/`), built only when the `OFOD_BUILD_COMPARE` CMake
  option is on (with the `compare` vcpkg feature, or Ubuntu's
  `libquantlib0-dev`), so the default build and CI do not depend on
  QuantLib.
- **Python against Python:** a `tools/compare/compare_python.py` script
  times `ofod`, `ofod_cpp`, `QuantLib` (Python), FinancePy, arch and
  vollib on the same cases, with one warm-up call per case for Numba's
  compilation. The comparison libraries go in
  `tools/compare/requirements.txt`, never in `pyproject.toml`.
- **Accuracy beside speed.** Every row records the price and its error
  against the reference column above. For Monte Carlo, the useful figure
  is standard error times the square root of time, not time alone.
- **Licences.** FinancePy and PyFENG are GPL and ORE is modified BSD.
  The harness only imports or links them at benchmark time and does not
  copy their code into the repository, so `ofod` itself is unaffected.
- **Results** go in `docs/perf/compare-*.json` and
  [Comparison results](COMPARISON.md), with each library's version.

## Order of work

1. Python harness with Tier 1 against `QuantLib` (Python), FinancePy,
   arch and vollib. It needs only `pip install`, so it runs in the cloud
   container.
2. The C++ QuantLib binary for the Tier 1 cases that are C++ in both
   libraries: trees, finite differences, Heston, Hull-White and the CDO.
3. Tier 2.
4. ORE exposure and CVA, and desktop timings, on the local Docker machine.
