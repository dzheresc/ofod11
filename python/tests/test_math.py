import math

import pytest

from ofod import math as ofod_math


def test_brent_and_newton() -> None:
    root = ofod_math.brent(lambda x: x * x - 2.0, 0.0, 2.0)
    assert root == pytest.approx(math.sqrt(2.0), abs=1e-12)
    assert ofod_math.brent(lambda x: x * x + 1.0, -1.0, 1.0) is None
    newton = ofod_math.newton(lambda x: x * x - 2.0, lambda x: 2.0 * x, 1.0)
    assert newton == pytest.approx(math.sqrt(2.0), abs=1e-14)


def test_linear_interpolator() -> None:
    f = ofod_math.LinearInterpolator([1.0, 2.0, 4.0], [10.0, 20.0, 0.0])
    assert f(1.5) == pytest.approx(15.0)
    assert f(3.0) == pytest.approx(10.0)
    assert f(0.0) == 10.0
    assert f(9.0) == 0.0
    with pytest.raises(ValueError):
        ofod_math.LinearInterpolator([1.0, 1.0], [1.0, 2.0])
