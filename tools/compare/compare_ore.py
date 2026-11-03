#!/usr/bin/env python3
"""Times ofod's exposure simulation and CVA against ORE on the same inputs.

ORE (Open Source Risk Engine) is the only open-source comparator for Hull's
Ch 9 exposure and CVA calculations. It is configured through XML files, so
this script writes a one-trade setup to a temporary directory: a long
equity forward on a flat market, with interest rate volatility at zero so
that its cross-asset model reduces to the geometric Brownian motion that
ofod simulates. Exposure then has a closed form,

    EE(t) = S N(d1) - K e^{-rT} N(d2),

a Black-Scholes call on S with strike K e^{-r(T - t)} and expiry t, and
errors are measured against that and against the integral of the closed
form for the CVA. See docs/COMPARISON.md for the results.

ORE ships its own QuantLib, so this is a separate script rather than a
case in compare_python.py. From the repository root, with ofod, ofod_cpp
and open-source-risk-engine installed:

    OMP_NUM_THREADS=1 python tools/compare/compare_ore.py \
        --out docs/perf/compare-ore.json
"""

# The XML templates below keep their long lines.
# ruff: noqa: E501

import argparse
from collections.abc import Callable
import csv
import dataclasses
import datetime
from importlib import metadata
import json
import math
import os
import pathlib
import platform
import statistics
import sys
import tempfile
import time

import numpy as np
import ORE as ore  # noqa: N813

import ofod
import ofod_cpp

SPOT = 100.0
STRIKE = 100.0
RATE = 0.05
VOLATILITY = 0.3
HAZARD = 0.02
RECOVERY = 0.4
# 73 steps of 15 days make T = 3 exactly. ORE's model time is Actual/Actual
# and its curves are Actual/365 here; the two agree on a span with no leap
# day, which rules out T = 5.
STEPS = 73
STEP_DAYS = 15
MATURITY = STEPS * STEP_DAYS / 365.0
PATHS = 10_000
ASOF = datetime.date(2025, 1, 1)
# ORE's reports round to whole currency units for exposure and two decimals
# for CVA, so its trade is this size and the results are scaled back.
QUANTITY = 1_000_000
SEED = 42

_MARKET_TENORS = ("1Y", "2Y", "5Y", "10Y", "20Y")


def _normal_cdf(x: float) -> float:
    return 0.5 * math.erfc(-x / math.sqrt(2.0))


def exact_exposure(t: float) -> float:
    """Discounted expected positive exposure of the forward at time t."""
    discounted_strike = STRIKE * math.exp(-RATE * MATURITY)
    if t <= 0.0:
        return max(SPOT - discounted_strike, 0.0)
    vol = VOLATILITY * math.sqrt(t)
    d1 = math.log(SPOT / discounted_strike) / vol + 0.5 * vol
    return SPOT * _normal_cdf(d1) - discounted_strike * _normal_cdf(d1 - vol)


def exact_cva() -> float:
    """(1 - R) times the integral of the exposure against default density."""
    nodes, weights = np.polynomial.legendre.leggauss(200)
    times = 0.5 * MATURITY * (nodes + 1.0)
    density = HAZARD * np.exp(-HAZARD * times)
    exposure = np.array([exact_exposure(t) for t in times])
    return float(
        (1.0 - RECOVERY) * 0.5 * MATURITY * np.sum(weights * density * exposure)
    )


def _grid() -> list[float]:
    return [(i + 1) * MATURITY / STEPS for i in range(STEPS)]


def _default_probability(t1: float, t2: float) -> float:
    return math.exp(-HAZARD * t1) - math.exp(-HAZARD * t2)


def exact_midpoint_cva() -> float:
    """Eq. 9.1 on the exact exposure, with ofod's interval averages."""
    grid = [0.0, *_grid()]
    return (1.0 - RECOVERY) * sum(
        _default_probability(a, b)
        * 0.5
        * (exact_exposure(a) + exact_exposure(b))
        for a, b in zip(grid, grid[1:], strict=False)
    )


def exact_end_cva() -> float:
    """ORE's rule on the exact exposure.

    ORE weights each interval by the exposure at its end, and that exposure
    is zero on the maturity date, so the last interval adds nothing.
    """
    grid = [0.0, *_grid()]
    return (1.0 - RECOVERY) * sum(
        _default_probability(a, b) * exact_exposure(b)
        for a, b in zip(grid[:-1], grid[1:-1], strict=False)
    )


def _midpoint_exposures(profile: list[float]) -> list[float]:
    """Averages each interval's ends, the v_i of eq. 9.1."""
    ends = [exact_exposure(0.0), *profile]
    return [0.5 * (a + b) for a, b in zip(ends, ends[1:], strict=False)]


def run_ofod(lib, seed: int = 1) -> tuple[list[float], float]:
    """Returns the exposure profile and CVA from ofod or ofod_cpp."""
    credit = lib.credit
    profile = credit.simulate_forward_exposure(
        SPOT, STRIKE, RATE, VOLATILITY, MATURITY, STEPS, PATHS, seed
    )
    exposure = list(profile.expected_exposure)
    curve = lib.curves.CreditCurve.flat(HAZARD)
    return exposure, credit.cva(
        list(profile.times), _midpoint_exposures(exposure), curve, RECOVERY
    )


def _ql_date(date: datetime.date) -> str:
    return date.strftime("%Y-%m-%d")


def _market_lines() -> str:
    day = ASOF.strftime("%Y%m%d")
    lines = [
        f"{day} EQUITY/PRICE/FWD/USD {SPOT}",
        f"{day} RECOVERY_RATE/RATE/CPTY/SR/USD {RECOVERY}",
        f"{day} EQUITY_OPTION/RATE_LNVOL/FWD/USD/5Y/ATMF {VOLATILITY}",
        f"{day} SWAPTION/RATE_LNVOL/USD/1Y/1Y/ATM 0.1",
    ]
    for tenor in _MARKET_TENORS:
        lines.append(f"{day} ZERO/RATE/USD/USD1D/A365/{tenor} {RATE}")
        lines.append(f"{day} HAZARD_RATE/RATE/CPTY/SR/USD/{tenor} {HAZARD}")
    return "\n".join(lines) + "\n"


_CONVENTIONS = """<?xml version="1.0"?>
<Conventions>
  <Zero>
    <Id>USD-ZERO</Id>
    <TenorBased>true</TenorBased>
    <DayCounter>A365</DayCounter>
    <Compounding>Continuous</Compounding>
    <CompoundingFrequency>Annual</CompoundingFrequency>
    <TenorCalendar>NullCalendar</TenorCalendar>
    <SpotLag>0</SpotLag>
    <SpotCalendar>NullCalendar</SpotCalendar>
    <RollConvention>F</RollConvention>
    <EOM>false</EOM>
  </Zero>
  <CDS>
    <Id>CDS</Id>
    <SettlementDays>1</SettlementDays>
    <Calendar>WeekendsOnly</Calendar>
    <Frequency>Quarterly</Frequency>
    <PaymentConvention>Following</PaymentConvention>
    <Rule>CDS2015</Rule>
    <DayCounter>A360</DayCounter>
    <SettlesAccrual>true</SettlesAccrual>
    <PaysAtDefaultTime>true</PaysAtDefaultTime>
  </CDS>
  <Swap>
    <Id>USD-3M-SWAP</Id>
    <FixedCalendar>US,UK</FixedCalendar>
    <FixedFrequency>Semiannual</FixedFrequency>
    <FixedConvention>MF</FixedConvention>
    <FixedDayCounter>30/360</FixedDayCounter>
    <Index>USD-LIBOR-3M</Index>
  </Swap>
  <SwapIndex>
    <Id>USD-CMS-1Y</Id>
    <Conventions>USD-3M-SWAP</Conventions>
  </SwapIndex>
  <SwapIndex>
    <Id>USD-CMS-30Y</Id>
    <Conventions>USD-3M-SWAP</Conventions>
  </SwapIndex>
</Conventions>
"""


def _curve_config() -> str:
    zero = "".join(
        f"<Quote>ZERO/RATE/USD/USD1D/A365/{t}</Quote>" for t in _MARKET_TENORS
    )
    hazard = "".join(
        f"<Quote>HAZARD_RATE/RATE/CPTY/SR/USD/{t}</Quote>"
        for t in _MARKET_TENORS
    )
    return f"""<?xml version="1.0"?>
<CurveConfiguration>
  <YieldCurves>
    <YieldCurve>
      <CurveId>USD1D</CurveId>
      <CurveDescription/>
      <Currency>USD</Currency>
      <DiscountCurve/>
      <Segments>
        <Direct>
          <Type>Zero</Type>
          <Quotes>{zero}</Quotes>
          <Conventions>USD-ZERO</Conventions>
        </Direct>
      </Segments>
      <InterpolationVariable>Discount</InterpolationVariable>
      <InterpolationMethod>LogLinear</InterpolationMethod>
      <YieldCurveDayCounter>A365</YieldCurveDayCounter>
      <Tolerance>1e-12</Tolerance>
    </YieldCurve>
  </YieldCurves>
  <DefaultCurves>
    <DefaultCurve>
      <CurveId>CPTY_SR_USD</CurveId>
      <CurveDescription/>
      <Currency>USD</Currency>
      <Type>HazardRate</Type>
      <DiscountCurve/>
      <DayCounter>A365</DayCounter>
      <RecoveryRate>RECOVERY_RATE/RATE/CPTY/SR/USD</RecoveryRate>
      <Quotes>{hazard}</Quotes>
      <Conventions>CDS</Conventions>
    </DefaultCurve>
  </DefaultCurves>
  <SwaptionVolatilities>
    <SwaptionVolatility>
      <CurveId>USD_SWPTN</CurveId>
      <CurveDescription/>
      <Dimension>ATM</Dimension>
      <VolatilityType>Lognormal</VolatilityType>
      <Extrapolation>Flat</Extrapolation>
      <DayCounter>A365</DayCounter>
      <Calendar>US</Calendar>
      <BusinessDayConvention>Following</BusinessDayConvention>
      <OptionTenors>1Y</OptionTenors>
      <SwapTenors>1Y</SwapTenors>
      <ShortSwapIndexBase>USD-CMS-1Y</ShortSwapIndexBase>
      <SwapIndexBase>USD-CMS-30Y</SwapIndexBase>
    </SwaptionVolatility>
  </SwaptionVolatilities>
  <EquityCurves>
    <EquityCurve>
      <CurveId>FWD</CurveId>
      <CurveDescription/>
      <Currency>USD</Currency>
      <ForecastingCurve>USD1D</ForecastingCurve>
      <Type>NoDividends</Type>
      <SpotQuote>EQUITY/PRICE/FWD/USD</SpotQuote>
      <Quotes/>
      <DayCounter>A365</DayCounter>
    </EquityCurve>
  </EquityCurves>
  <EquityVolatilities>
    <EquityVolatility>
      <CurveId>FWD</CurveId>
      <CurveDescription/>
      <Currency>USD</Currency>
      <DayCounter>A365</DayCounter>
      <Calendar>NullCalendar</Calendar>
      <Constant>
        <Quote>EQUITY_OPTION/RATE_LNVOL/FWD/USD/5Y/ATMF</Quote>
      </Constant>
    </EquityVolatility>
  </EquityVolatilities>
</CurveConfiguration>
"""


_TODAYS_MARKET = """<?xml version="1.0"?>
<TodaysMarket>
  <Configuration id="default"/>
  <DiscountingCurves id="default">
    <DiscountingCurve currency="USD">Yield/USD/USD1D</DiscountingCurve>
  </DiscountingCurves>
  <IndexForwardingCurves id="default">
    <Index name="USD-FedFunds">Yield/USD/USD1D</Index>
    <Index name="USD-SOFR">Yield/USD/USD1D</Index>
    <Index name="USD-LIBOR-3M">Yield/USD/USD1D</Index>
  </IndexForwardingCurves>
  <SwapIndexCurves id="default">
    <SwapIndex name="USD-CMS-1Y"><Discounting>USD-FedFunds</Discounting></SwapIndex>
    <SwapIndex name="USD-CMS-30Y"><Discounting>USD-FedFunds</Discounting></SwapIndex>
  </SwapIndexCurves>
  <SwaptionVolatilities id="default">
    <SwaptionVolatility currency="USD">SwaptionVolatility/USD/USD_SWPTN</SwaptionVolatility>
  </SwaptionVolatilities>
  <EquityCurves id="default">
    <EquityCurve name="FWD">Equity/USD/FWD</EquityCurve>
  </EquityCurves>
  <EquityVolatilities id="default">
    <EquityVolatility name="FWD">EquityVolatility/USD/FWD</EquityVolatility>
  </EquityVolatilities>
  <DefaultCurves id="default">
    <DefaultCurve name="CPTY">Default/USD/CPTY_SR_USD</DefaultCurve>
  </DefaultCurves>
</TodaysMarket>
"""

_PRICING_ENGINES = """<?xml version="1.0"?>
<PricingEngines>
  <Product type="EquityForward">
    <Model>DiscountedCashflows</Model>
    <ModelParameters/>
    <Engine>DiscountingEquityForwardEngine</Engine>
    <EngineParameters/>
  </Product>
</PricingEngines>
"""


def _portfolio() -> str:
    maturity = ASOF + datetime.timedelta(days=STEPS * STEP_DAYS)
    return f"""<?xml version="1.0"?>
<Portfolio>
  <Trade id="FWD">
    <TradeType>EquityForward</TradeType>
    <Envelope>
      <CounterParty>CPTY</CounterParty>
      <NettingSetId>CPTY</NettingSetId>
      <AdditionalFields/>
    </Envelope>
    <EquityForwardData>
      <LongShort>Long</LongShort>
      <Maturity>{_ql_date(maturity)}</Maturity>
      <Name>FWD</Name>
      <Currency>USD</Currency>
      <Strike>{STRIKE}</Strike>
      <Quantity>{QUANTITY}</Quantity>
    </EquityForwardData>
  </Trade>
</Portfolio>
"""


_NETTING = """<?xml version="1.0"?>
<NettingSetDefinitions>
  <NettingSet>
    <NettingSetId>CPTY</NettingSetId>
    <ActiveCSAFlag>false</ActiveCSAFlag>
  </NettingSet>
</NettingSetDefinitions>
"""


def _simulation(sequence: str) -> str:
    return f"""<?xml version="1.0"?>
<Simulation>
  <Parameters>
    <Discretization>Exact</Discretization>
    <Grid>{STEPS},{STEP_DAYS}D</Grid>
    <Calendar>NullCalendar</Calendar>
    <Sequence>{sequence}</Sequence>
    <Scenario>Simple</Scenario>
    <Seed>{SEED}</Seed>
    <Samples>{PATHS}</Samples>
    <Ordering>Steps</Ordering>
    <DirectionIntegers>JoeKuoD7</DirectionIntegers>
  </Parameters>
  <CrossAssetModel>
    <DomesticCcy>USD</DomesticCcy>
    <Currencies><Currency>USD</Currency></Currencies>
    <Equities><Equity>FWD</Equity></Equities>
    <BootstrapTolerance>0.0001</BootstrapTolerance>
    <InterestRateModels>
      <LGM ccy="default">
        <CalibrationType>None</CalibrationType>
        <Volatility>
          <Calibrate>N</Calibrate>
          <VolatilityType>Hagan</VolatilityType>
          <ParamType>Constant</ParamType>
          <TimeGrid/>
          <InitialValue>0.0</InitialValue>
        </Volatility>
        <Reversion>
          <Calibrate>N</Calibrate>
          <ReversionType>HullWhite</ReversionType>
          <ParamType>Constant</ParamType>
          <TimeGrid/>
          <InitialValue>0.03</InitialValue>
        </Reversion>
        <CalibrationSwaptions>
          <Expiries>1Y</Expiries>
          <Terms>1Y</Terms>
          <Strikes/>
        </CalibrationSwaptions>
        <ParameterTransformation>
          <ShiftHorizon>0.0</ShiftHorizon>
          <Scaling>1.0</Scaling>
        </ParameterTransformation>
      </LGM>
    </InterestRateModels>
    <EquityModels>
      <CrossAssetLGM name="default">
        <Currency>USD</Currency>
        <CalibrationType>None</CalibrationType>
        <Sigma>
          <Calibrate>N</Calibrate>
          <ParamType>Constant</ParamType>
          <TimeGrid/>
          <InitialValue>{VOLATILITY}</InitialValue>
        </Sigma>
        <CalibrationOptions>
          <Expiries>1Y</Expiries>
          <Strikes/>
        </CalibrationOptions>
      </CrossAssetLGM>
    </EquityModels>
    <InstantaneousCorrelations>
      <Correlation factor1="IR:USD" factor2="EQ:FWD">0.0</Correlation>
    </InstantaneousCorrelations>
  </CrossAssetModel>
  <Market>
    <BaseCurrency>USD</BaseCurrency>
    <Currencies><Currency>USD</Currency></Currencies>
    <YieldCurves>
      <Configuration>
        <Tenors>3M,6M,1Y,2Y,3Y,4Y,5Y,7Y,10Y</Tenors>
        <Interpolation>LogLinear</Interpolation>
        <Extrapolation>Y</Extrapolation>
      </Configuration>
    </YieldCurves>
    <Indices>
      <Index>USD-FedFunds</Index>
      <Index>USD-LIBOR-3M</Index>
      <Index>USD-SOFR</Index>
    </Indices>
    <SwapIndices>
      <SwapIndex>
        <Name>USD-CMS-1Y</Name>
        <DiscountingIndex>USD-FedFunds</DiscountingIndex>
      </SwapIndex>
      <SwapIndex>
        <Name>USD-CMS-30Y</Name>
        <DiscountingIndex>USD-FedFunds</DiscountingIndex>
      </SwapIndex>
    </SwapIndices>
    <DefaultCurves>
      <Names/>
      <Tenors>1Y,5Y,10Y</Tenors>
    </DefaultCurves>
    <Equities>
      <SimulateDividendYield>false</SimulateDividendYield>
      <Names><Name>FWD</Name></Names>
      <DividendTenors>1Y,5Y</DividendTenors>
    </Equities>
    <EquityVolatilities>
      <Simulate>false</Simulate>
      <ReactionToTimeDecay>ForwardVariance</ReactionToTimeDecay>
      <Names><Name>FWD</Name></Names>
      <Expiries>1Y,5Y,10Y</Expiries>
    </EquityVolatilities>
    <AggregationScenarioDataCurrencies>
      <Currency>USD</Currency>
    </AggregationScenarioDataCurrencies>
    <AggregationScenarioDataIndices>
      <Index>USD-FedFunds</Index>
    </AggregationScenarioDataIndices>
  </Market>
</Simulation>
"""


def _ore_xml(directory: pathlib.Path) -> str:
    return f"""<?xml version="1.0"?>
<ORE>
  <Setup>
    <Parameter name="asofDate">{_ql_date(ASOF)}</Parameter>
    <Parameter name="inputPath">{directory}</Parameter>
    <Parameter name="outputPath">{directory}/out</Parameter>
    <Parameter name="logFile">log.txt</Parameter>
    <Parameter name="logMask">7</Parameter>
    <Parameter name="marketDataFile">market.txt</Parameter>
    <Parameter name="fixingDataFile">fixings.txt</Parameter>
    <Parameter name="implyTodaysFixings">Y</Parameter>
    <Parameter name="curveConfigFile">curveconfig.xml</Parameter>
    <Parameter name="conventionsFile">conventions.xml</Parameter>
    <Parameter name="marketConfigFile">todaysmarket.xml</Parameter>
    <Parameter name="pricingEnginesFile">pricingengine.xml</Parameter>
    <Parameter name="portfolioFile">portfolio.xml</Parameter>
    <Parameter name="observationModel">Disable</Parameter>
  </Setup>
  <Markets/>
  <Analytics>
    <Analytic type="simulation">
      <Parameter name="active">Y</Parameter>
      <Parameter name="simulationConfigFile">simulation.xml</Parameter>
      <Parameter name="pricingEnginesFile">pricingengine.xml</Parameter>
      <Parameter name="baseCurrency">USD</Parameter>
      <Parameter name="storeScenarios">N</Parameter>
    </Analytic>
    <Analytic type="xva">
      <Parameter name="active">Y</Parameter>
      <Parameter name="csaFile">netting.xml</Parameter>
      <Parameter name="baseCurrency">USD</Parameter>
      <Parameter name="exposureProfiles">Y</Parameter>
      <Parameter name="exposureProfilesByTrade">Y</Parameter>
      <Parameter name="quantile">0.975</Parameter>
      <Parameter name="calculationType">Symmetric</Parameter>
      <Parameter name="allocationMethod">None</Parameter>
      <Parameter name="marginalAllocationLimit">1.0</Parameter>
      <Parameter name="exerciseNextBreak">N</Parameter>
      <Parameter name="cva">Y</Parameter>
      <Parameter name="dva">N</Parameter>
      <Parameter name="fva">N</Parameter>
      <Parameter name="colva">N</Parameter>
      <Parameter name="collateralFloor">N</Parameter>
    </Analytic>
  </Analytics>
</ORE>
"""


def write_inputs(directory: pathlib.Path, sequence: str) -> pathlib.Path:
    """Writes the ORE input files and returns the path of ore.xml."""
    (directory / "out").mkdir(exist_ok=True)
    files = {
        "market.txt": _market_lines(),
        "fixings.txt": "",
        "conventions.xml": _CONVENTIONS,
        "curveconfig.xml": _curve_config(),
        "todaysmarket.xml": _TODAYS_MARKET,
        "pricingengine.xml": _PRICING_ENGINES,
        "portfolio.xml": _portfolio(),
        "netting.xml": _NETTING,
        "simulation.xml": _simulation(sequence),
        "ore.xml": _ore_xml(directory),
    }
    for name, text in files.items():
        (directory / name).write_text(text, encoding="utf-8")
    return directory / "ore.xml"


def run_ore(xml: pathlib.Path) -> tuple[list[float], float]:
    """Runs ORE; returns EPE on the grid (without t = 0) and the CVA."""
    params = ore.Parameters()
    params.fromFile(str(xml))
    app = ore.OREApp(params)
    app.run()
    out = xml.parent / "out"
    with open(out / "exposure_trade_FWD.csv", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    with open(out / "xva.csv", encoding="utf-8") as f:
        xva = list(csv.DictReader(f))
    exposure = [
        float(r["EPE"]) / QUANTITY for r in rows if float(r["Time"]) > 0.0
    ]
    return exposure, float(xva[0]["CVA"]) / QUANTITY


@dataclasses.dataclass
class Row:
    """One library's result."""

    library: str
    seconds: float
    cva: float
    cva_error: float
    same_rule_cva_error: float
    max_exposure_error: float
    rms_exposure_error: float
    note: str = ""


def _measure(
    library: str,
    fn: Callable[[], tuple[list[float], float]],
    repeat: int,
    same_rule_cva: float,
    note: str = "",
) -> Row:
    fn()
    times = []
    for _ in range(repeat):
        start = time.perf_counter()
        exposure, cva = fn()
        times.append(time.perf_counter() - start)
    # ORE values a trade on its maturity date at zero, so the last grid
    # date is left out for every library.
    errors = [
        e - exact_exposure(t)
        for e, t in zip(exposure[: STEPS - 1], _grid(), strict=False)
    ]
    row = Row(
        library=library,
        seconds=statistics.median(times),
        cva=cva,
        cva_error=cva - exact_cva(),
        same_rule_cva_error=cva - same_rule_cva,
        max_exposure_error=max(abs(e) for e in errors),
        rms_exposure_error=math.sqrt(sum(e * e for e in errors) / len(errors)),
        note=note,
    )
    print(
        f"| {row.library} | {row.seconds:.4g} s | {row.cva:.5f} | "
        f"{row.cva_error:.2e} | {row.same_rule_cva_error:.2e} | "
        f"{row.max_exposure_error:.3f} | "
        f"{row.rms_exposure_error:.3f} |",
        flush=True,
    )
    return row


def main() -> int:
    """Runs every library and prints a Markdown table."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", help="write results as JSON to this path")
    parser.add_argument("--repeat", type=int, default=5)
    args = parser.parse_args()

    print(f"Exact CVA {exact_cva():.6f}, EE(T/2) {exact_exposure(2.5):.6f}")
    print(
        "| Library | Time | CVA | CVA error | Same-rule CVA error | "
        "Max EE error | RMS EE error |"
    )
    print("| --- | --- | --- | --- | --- | --- | --- |")
    rows = [
        _measure(
            "ofod (Python)",
            lambda: run_ofod(ofod),
            args.repeat,
            exact_midpoint_cva(),
        ),
        _measure(
            "ofod_cpp",
            lambda: run_ofod(ofod_cpp),
            args.repeat,
            exact_midpoint_cva(),
        ),
    ]
    with tempfile.TemporaryDirectory() as tmp:
        for sequence in ("MersenneTwister", "SobolBrownianBridge"):
            directory = pathlib.Path(tmp) / sequence
            directory.mkdir()
            xml = write_inputs(directory, sequence)
            rows.append(
                _measure(
                    f"ORE ({sequence})",
                    lambda x=xml: run_ore(x),
                    args.repeat,
                    exact_end_cva(),
                )
            )
    if args.out:
        versions = {
            "python": platform.python_version(),
            "open-source-risk-engine": metadata.version(
                "open-source-risk-engine"
            ),
        }
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(
                {
                    "machine": platform.machine(),
                    "cpus": os.cpu_count(),
                    "versions": versions,
                    "exact_cva": exact_cva(),
                    "exact_midpoint_cva": exact_midpoint_cva(),
                    "exact_end_cva": exact_end_cva(),
                    "rows": [dataclasses.asdict(r) for r in rows],
                },
                f,
                indent=2,
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
