"""Python bindings for the ofod11 C++ library.

The public API mirrors the pure-Python `ofod` package: the same submodules,
classes and function names, so either can be passed where the other is
expected.
"""

import sys

from ofod_cpp import _core

core = _core.core
curves = _core.curves
instruments = _core.instruments
math = _core.math
pricers = _core.pricers

# Make `import ofod_cpp.core` work like it does for the pure-Python package.
for _name in ("core", "curves", "instruments", "math", "pricers"):
    sys.modules[f"{__name__}.{_name}"] = getattr(_core, _name)

__all__ = ["core", "curves", "instruments", "math", "pricers"]
