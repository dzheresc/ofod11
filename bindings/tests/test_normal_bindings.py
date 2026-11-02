from hypothesis import given
from hypothesis import strategies as st
import pytest

from ofod.math import normal as py_normal
from ofod_cpp import math as cpp_math


def test_table_value() -> None:
    assert cpp_math.norm_cdf(0.6278) == pytest.approx(0.7349, abs=1e-4)


@given(st.floats(min_value=-8.0, max_value=8.0))
def test_cpp_matches_python(x: float) -> None:
    assert cpp_math.norm_cdf(x) == pytest.approx(
        py_normal.norm_cdf(x), abs=1e-15
    )
    assert cpp_math.norm_cdf_poly(x) == pytest.approx(
        py_normal.norm_cdf_poly(x), abs=1e-14
    )
