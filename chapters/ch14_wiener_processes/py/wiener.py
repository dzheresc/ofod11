"""Chapter 14: GBM simulation, quadratic variation, correlation, Ito.

Run with `python chapters/ch14_wiener_processes/py/wiener.py`.
Pass --cpp to use the C++ bindings instead of the pure-Python library.
Random draws differ between the two libraries; Table 14.1 does not.
"""

import argparse
import importlib
import math


def main() -> None:
    """Reproduces the Chapter 14 examples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", action="store_true", help="use ofod_cpp")
    lib = importlib.import_module(
        "ofod_cpp" if parser.parse_args().cpp else "ofod"
    )
    models = lib.models
    gbm = models.GeometricBrownianMotion(0.15, 0.30)
    path = gbm.euler_path(100.0, 0.0192, [0.52, 1.44, -0.86, 1.46, -0.69])
    print("Table 14.1: S over five weeks:", " ".join(f"{s:.2f}" for s in path))

    gen = lib.random.NormalGenerator(2026)
    w = models.ArithmeticBrownianMotion(0.0, 1.0)
    for n in (10, 1000, 100_000):
        qv = models.quadratic_variation(w.path(0.0, 1.0 / n, gen.sample(n)))
        print(
            f"Wiener path on [0, 1], {n:>6} steps: quadratic variation {qv:.4f}"
        )

    lower = lib.random.cholesky([[1.0, 0.6], [0.6, 1.0]])
    e = lib.random.correlate(lower, [1.0, 2.0])
    print(f"rho = 0.6: (1, 2) -> ({e[0]:.2f}, {e[1]:.2f})")

    stock = models.GeometricBrownianMotion(0.12, 0.4)
    paths = 5000
    total = sum(
        math.log(stock.euler_path(1.0, 1.0 / 250, gen.sample(250))[-1])
        for _ in range(paths)
    )
    print(
        f"Ito: mean ln(S_1/S_0) = {total / paths:.4f} "
        f"(s.e. {0.4 / math.sqrt(paths):.4f}), "
        f"mu - sigma^2/2 = {0.12 - 0.5 * 0.4**2:.4f}"
    )


if __name__ == "__main__":
    main()
