"""Small dense linear algebra: the symmetric eigenproblem (cyclic Jacobi).

The algorithm and operation order match the C++ library, so both give the
same eigenvalues and eigenvectors to rounding.
"""

from collections.abc import Sequence
import dataclasses
import math

Matrix = list[list[float]]

_MAX_SWEEPS = 100


@dataclasses.dataclass(frozen=True)
class Eigen:
    """Eigenvalues (descending) and unit eigenvectors.

    `vectors[k]` belongs to `values[k]` and is signed so its entries sum to
    a non-negative number.
    """

    values: list[float]
    vectors: Matrix


def _off_diagonal_norm(a: Matrix) -> float:
    n = len(a)
    total = 0.0
    for i in range(n):
        for j in range(i + 1, n):
            total += a[i][j] * a[i][j]
    return math.sqrt(total)


def symmetric_eigen(matrix: Sequence[Sequence[float]]) -> Eigen:
    """Eigen-decomposition of a symmetric matrix by Jacobi rotations.

    Raises:
        ValueError: If the matrix is empty, not square, not symmetric (to
            1e-12 relative) or the sweeps do not converge.
    """
    n = len(matrix)
    if n == 0 or any(len(row) != n for row in matrix):
        raise ValueError("matrix must be square and non-empty")
    scale = max(abs(x) for row in matrix for x in row)
    for i in range(n):
        for j in range(i + 1, n):
            if abs(matrix[i][j] - matrix[j][i]) > 1e-12 * max(scale, 1.0):
                raise ValueError("matrix must be symmetric")
    a = [[float(x) for x in row] for row in matrix]
    v = [[1.0 if i == j else 0.0 for j in range(n)] for i in range(n)]
    tol = 1e-15 * max(scale, 1e-300)
    sweep = 0
    while sweep < _MAX_SWEEPS and _off_diagonal_norm(a) > tol:
        for p in range(n):
            for q in range(p + 1, n):
                if a[p][q] == 0.0:
                    continue
                theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q])
                t = math.copysign(1.0, theta) / (
                    abs(theta) + math.sqrt(theta * theta + 1.0)
                )
                c = 1.0 / math.sqrt(t * t + 1.0)
                s = t * c
                for k in range(n):
                    akp, akq = a[k][p], a[k][q]
                    a[k][p] = c * akp - s * akq
                    a[k][q] = s * akp + c * akq
                for k in range(n):
                    apk, aqk = a[p][k], a[q][k]
                    a[p][k] = c * apk - s * aqk
                    a[q][k] = s * apk + c * aqk
                for k in range(n):
                    vkp, vkq = v[k][p], v[k][q]
                    v[k][p] = c * vkp - s * vkq
                    v[k][q] = s * vkp + c * vkq
        sweep += 1
    if _off_diagonal_norm(a) > tol:
        raise ValueError("Jacobi sweeps did not converge")
    order = sorted(range(n), key=lambda i: -a[i][i])
    values, vectors = [], []
    for k in order:
        values.append(a[k][k])
        column = [v[i][k] for i in range(n)]
        total = 0.0
        for x in column:
            total += x
        if total < 0.0:
            column = [-x for x in column]
        vectors.append(column)
    return Eigen(values, vectors)
