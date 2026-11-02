# Chapter 25: Credit derivatives

CDS valuation with defaults at mid-period and accrued premium (25.2), the
mark-to-market of an existing CDS and the upfront for a fixed coupon
(25.3, 25.4), bootstrapping piecewise hazards from a CDS term structure,
CDS options with Black's model (25.7), CDO tranche spreads in the
one-factor Gaussian copula with Gauss-Hermite quadrature (25.10), implied
(compound) correlation (25.11) and nth-to-default baskets by Monte Carlo
(25.9). Golden tests: `data/golden/ch25.json`.

| Language | Example | Library code |
| --- | --- | --- |
| C++ | `cpp/cds.cc` | `ofod/credit/cds.h`, `ofod/credit/copula.h` |
| Python | `py/cds.py [--cpp]` | `ofod.credit.cds`, `ofod.credit.copula` |

The CDS example uses a flat 2% hazard and annual payments: the par spread
comes out at 123.0 bp, close to the s = 0.02 x 0.6 = 120 bp of the credit
triangle. Hull's tables use discrete default probabilities per year, so
their figures differ slightly; the golden values are marked "computed".

Tranche losses use the conditional default-count recursion of Andersen,
Sidenius and Basu (vectorised over quadrature nodes in Python). Mezzanine
spreads are not monotone in correlation, which is why the 3-6% tranche
has two correlations for some quotes; the implied correlation search
returns the lowest. The nth-to-default lines agree between C++ and pure
Python only within Monte Carlo error; `py --cpp` matches the C++ output
exactly.
