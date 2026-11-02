# Golden test data

One JSON file per chapter. Each case reproduces a number printed in Hull,
*Options, Futures, and Other Derivatives*. The C++ tests, the Python tests
and the bindings tests all read these files, so each number is typed once.

Case fields:

| Field | Meaning |
| --- | --- |
| `id` | Unique name within the file |
| `ref` | Where the number appears in the book |
| `kind` | Which function the case exercises; each test suite dispatches on it |
| `inputs` | Arguments, in book units (rates as decimals, times in years) |
| `expected` | The book's value (a number, or a list for curve outputs) |
| `tol` | Absolute tolerance, matching the precision the book prints |

The references were checked against the worked examples as remembered from
the 10th and 11th editions; if a number differs from your copy of the 11th
edition, fix it here and both languages pick it up.
