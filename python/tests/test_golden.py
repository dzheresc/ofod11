import pytest

import ofod
from ofod.testing import golden

_CASES = list(golden.iter_cases(2, 3, 4, 5, 6, 7))


@pytest.mark.parametrize("case", _CASES, ids=[c["id"] for c in _CASES])
def test_golden(case: dict) -> None:
    golden.check_case(ofod, case)
