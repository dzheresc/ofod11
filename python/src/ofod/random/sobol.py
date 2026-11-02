"""Sobol low-discrepancy sequence (Hull, Ch 21.7).

Same direction numbers and Gray-code ordering as the C++ library, so both
produce identical points.
"""

_BITS = 32
_SCALE = 1.0 / 4294967296.0
# (degree, a, m) for dimensions 2..8 (Joe-Kuo).
_PRIMITIVES = (
    (1, 0, (1,)),
    (2, 1, (1, 3)),
    (3, 1, (1, 3, 1)),
    (3, 2, (1, 1, 1)),
    (4, 1, (1, 1, 3, 3)),
    (4, 4, (1, 3, 5, 13)),
    (5, 2, (1, 1, 5, 5, 17)),
)


class Sobol:
    """Points in [0, 1)^d; the all-zero first point is skipped."""

    MAX_DIMENSIONS = 8

    def __init__(self, dimensions: int) -> None:
        """Creates a sequence in `dimensions` dimensions.

        Raises:
            ValueError: Unless 1 <= dimensions <= 8.
        """
        if not 1 <= dimensions <= self.MAX_DIMENSIONS:
            raise ValueError("dimensions must be in [1, 8]")
        self._dimensions = dimensions
        self._index = 0
        self._state = [0] * dimensions
        self._directions = [[1 << (_BITS - 1 - k) for k in range(_BITS)]]
        for degree, a, m in _PRIMITIVES[: dimensions - 1]:
            v = [m[k] << (_BITS - 1 - k) for k in range(degree)]
            for k in range(degree, _BITS):
                value = v[k - degree] ^ (v[k - degree] >> degree)
                for j in range(1, degree):
                    if (a >> (degree - 1 - j)) & 1:
                        value ^= v[k - j]
                v.append(value)
            self._directions.append(v)

    @property
    def dimensions(self) -> int:
        """Returns the number of dimensions."""
        return self._dimensions

    def next(self) -> list[float]:
        """Returns the next point."""
        c = 0
        while (self._index >> c) & 1:
            c += 1
        self._index += 1
        point = []
        for d in range(self._dimensions):
            self._state[d] ^= self._directions[d][c]
            point.append(self._state[d] * _SCALE)
        return point
