import pytest

from ofod.testing import golden
import ofod_cpp

_CASES = list(golden.iter_cases(2, 3, 4, 5, 6, 7, 10, 11, 12, 13))


@pytest.mark.parametrize("case", _CASES, ids=[c["id"] for c in _CASES])
def test_golden(case: dict) -> None:
    golden.check_case(ofod_cpp, case)
