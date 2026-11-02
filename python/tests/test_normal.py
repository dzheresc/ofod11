from hypothesis import given
from hypothesis import strategies as st
import pytest

from ofod.math import normal


@pytest.mark.parametrize(
    ("x", "expected"),
    [(0.0, 0.5), (0.6278, 0.7349), (-0.12, 0.4522), (1.96, 0.9750)],
)
def test_norm_cdf_table(x: float, expected: float) -> None:
    assert normal.norm_cdf(x) == pytest.approx(expected, abs=1e-4)


@given(st.floats(min_value=-8.0, max_value=8.0))
def test_poly_matches_erfc(x: float) -> None:
    assert normal.norm_cdf_poly(x) == pytest.approx(
        normal.norm_cdf(x), abs=7.5e-8
    )


@given(st.floats(min_value=0.0, max_value=8.0))
def test_symmetry(x: float) -> None:
    total = normal.norm_cdf(x) + normal.norm_cdf(-x)
    assert total == pytest.approx(1.0, abs=1e-15)
