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


def solve_linear(
    a: Sequence[Sequence[float]], b: Sequence[float]
) -> list[float]:
    """Solves A x = b (Gaussian elimination with partial pivoting).

    Raises:
        ValueError: Unless A is square, matches b and is non-singular.
    """
    n = len(a)
    if n == 0 or len(b) != n or any(len(row) != n for row in a):
        raise ValueError("A must be square and match b")
    m = [list(map(float, row)) for row in a]
    rhs = [float(v) for v in b]
    for col in range(n):
        pivot = max(range(col, n), key=lambda r: abs(m[r][col]))
        if m[pivot][col] == 0.0:
            raise ValueError("matrix is singular")
        m[col], m[pivot] = m[pivot], m[col]
        rhs[col], rhs[pivot] = rhs[pivot], rhs[col]
        for r in range(col + 1, n):
            f = m[r][col] / m[col][col]
            for c in range(col, n):
                m[r][c] -= f * m[col][c]
            rhs[r] -= f * rhs[col]
    x = [0.0] * n
    for i in range(n - 1, -1, -1):
        total = rhs[i] - sum(m[i][c] * x[c] for c in range(i + 1, n))
        x[i] = total / m[i][i]
    return x
