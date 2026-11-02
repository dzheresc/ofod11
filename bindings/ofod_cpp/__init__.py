"""Python bindings for the ofod11 C++ library.

The public API mirrors the pure-Python `ofod` package: the same submodules,
classes and function names, so either can be passed where the other is
expected.
"""

import sys

from ofod_cpp import _core

core = _core.core
credit = _core.credit
curves = _core.curves
futures = _core.futures
instruments = _core.instruments
math = _core.math
models = _core.models
pricers = _core.pricers
random = _core.random
risk = _core.risk
stats = _core.stats
strategies = _core.strategies

# Make `import ofod_cpp.core` work like it does for the pure-Python package.
for _name in (
    "core",
    "credit",
    "curves",
    "futures",
    "instruments",
    "math",
    "models",
    "pricers",
    "random",
    "risk",
    "stats",
    "strategies",
):
    sys.modules[f"{__name__}.{_name}"] = getattr(_core, _name)

__all__ = [
    "core",
    "credit",
    "curves",
    "futures",
    "instruments",
    "math",
    "models",
    "pricers",
    "random",
    "risk",
    "stats",
    "strategies",
]
