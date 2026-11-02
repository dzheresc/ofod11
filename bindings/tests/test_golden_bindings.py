import pytest

from ofod.testing import golden
import ofod_cpp

_CHAPTERS = (
    2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25,
)  # fmt: skip
_CASES = list(golden.iter_cases(*_CHAPTERS))


@pytest.mark.parametrize("case", _CASES, ids=[c["id"] for c in _CASES])
def test_golden(case: dict) -> None:
    golden.check_case(ofod_cpp, case)
