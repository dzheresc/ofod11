"""Standard normal distribution helpers (Hull, Ch 15.9)."""

import math

_INV_SQRT_2PI = 1.0 / math.sqrt(2.0 * math.pi)

# Abramowitz-Stegun 26.2.17 coefficients, as printed in Hull.
_GAMMA = 0.2316419
_A1 = 0.319381530
_A2 = -0.356563782
_A3 = 1.781477937
_A4 = -1.821255978
_A5 = 1.330274429


def norm_pdf(x: float) -> float:
    """Computes the standard normal probability density.

    Args:
        x: Point at which to evaluate the density.

    Returns:
        The density phi(x).
    """
    return math.exp(-0.5 * x * x) * _INV_SQRT_2PI


def norm_cdf(x: float) -> float:
    """Computes the standard normal cumulative distribution N(x).

    Args:
        x: Upper integration limit.

    Returns:
        N(x), computed with erfc to full double precision.
    """
    return 0.5 * math.erfc(-x / math.sqrt(2.0))


def norm_cdf_poly(x: float) -> float:
    """Computes Hull's polynomial approximation to N(x).

    Args:
        x: Upper integration limit.

    Returns:
        N(x) with absolute error below 7.5e-8.
    """
    if x < 0.0:
        return 1.0 - norm_cdf_poly(-x)
    k = 1.0 / (1.0 + _GAMMA * x)
    poly = k * (_A1 + k * (_A2 + k * (_A3 + k * (_A4 + k * _A5))))
    return 1.0 - norm_pdf(x) * poly


_INV_A = (
    -3.969683028665376e01,
    2.209460984245205e02,
    -2.759285104469687e02,
    1.383577518672690e02,
    -3.066479806614716e01,
    2.506628277459239e00,
)
_INV_B = (
    -5.447609879822406e01,
    1.615858368580409e02,
    -1.556989798598866e02,
    6.680131188771972e01,
    -1.328068155288572e01,
)
_INV_C = (
    -7.784894002430293e-03,
    -3.223964580411365e-01,
    -2.400758277161838e00,
    -2.549732539343734e00,
    4.374664141464968e00,
    2.938163982698783e00,
)
_INV_D = (
    7.784695709041462e-03,
    3.224671290700398e-01,
    2.445134137142996e00,
    3.754408661907416e00,
)
_INV_LOW = 0.02425


def norm_inv(p: float) -> float:
    """Returns x with N(x) = p (Acklam plus one Halley step).

    Returns -inf / +inf at p = 0 / 1 and NaN outside [0, 1].
    """
    if math.isnan(p) or p < 0.0 or p > 1.0:
        return math.nan
    if p == 0.0:
        return -math.inf
    if p == 1.0:
        return math.inf
    a, b, c, d = _INV_A, _INV_B, _INV_C, _INV_D
    if p < _INV_LOW or p > 1.0 - _INV_LOW:
        q = math.sqrt(-2.0 * math.log(p if p < _INV_LOW else 1.0 - p))
        x = (
            ((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]
        ) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0)
        if p > 1.0 - _INV_LOW:
            x = -x
    else:
        q = p - 0.5
        r = q * q
        x = (
            (
                ((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r
                + a[5]
            )
            * q
            / (
                ((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r
                + 1.0
            )
        )
    e = norm_cdf(x) - p
    u = e / norm_pdf(x)
    return x - u / (1.0 + 0.5 * x * u)


# Gauss-Legendre nodes (negative half) and weights for 6, 12, 20 points.
_GL = (
    (
        (-0.9324695142031522, -0.6612093864662647, -0.2386191860831970),
        (0.1713244923791705, 0.3607615730481384, 0.4679139345726904),
    ),
    (
        (
            -0.9815606342467191, -0.9041172563704750, -0.7699026741943050,
            -0.5873179542866171, -0.3678314989981802, -0.1252334085114692,
        ),
        (
            0.04717533638651177, 0.1069393259953183, 0.1600783285433464,
            0.2031674267230659, 0.2334925365383547, 0.2491470458134029,
        ),
    ),
    (
        (
            -0.9931285991850949, -0.9639719272779138, -0.9122344282513259,
            -0.8391169718222188, -0.7463319064601508, -0.6360536807265150,
            -0.5108670019508271, -0.3737060887154196, -0.2277858511416451,
            -0.07652652113349733,
        ),
        (
            0.01761400713915212, 0.04060142980038694, 0.06267204833410906,
            0.08327674157670475, 0.1019301198172404, 0.1181945319615184,
            0.1316886384491766, 0.1420961093183821, 0.1491729864726037,
            0.1527533871307259,
        ),
    ),
)  # fmt: skip


def _upper_bivariate(h: float, k: float, r: float) -> float:
    """P(X > h, Y > k) with correlation r (Genz 2004, "bvnu")."""
    x, w = _GL[0] if abs(r) < 0.3 else _GL[1] if abs(r) < 0.75 else _GL[2]
    two_pi = 2.0 * math.pi
    hk = h * k
    bvn = 0.0
    if abs(r) < 0.925:
        hs = (h * h + k * k) / 2.0
        asr = math.asin(r)
        for i in range(len(x)):
            for sign in (-1.0, 1.0):
                sn = math.sin(asr * (1.0 + sign * x[i]) / 2.0)
                bvn += w[i] * math.exp((sn * hk - hs) / (1.0 - sn * sn))
        return bvn * asr / (2.0 * two_pi) + norm_cdf(-h) * norm_cdf(-k)
    if r < 0.0:
        k, hk = -k, -hk
    if abs(r) < 1.0:
        as_ = (1.0 - r) * (1.0 + r)
        a = math.sqrt(as_)
        bs = (h - k) * (h - k)
        c = (4.0 - hk) / 8.0
        d = (12.0 - hk) / 16.0
        asr = -(bs / as_ + hk) / 2.0
        if asr > -100.0:
            bvn = (
                a
                * math.exp(asr)
                * (
                    1.0
                    - c * (bs - as_) * (1.0 - d * bs / 5.0) / 3.0
                    + c * d * as_ * as_ / 5.0
                )
            )
        if hk > -100.0:
            b = math.sqrt(bs)
            sp = math.sqrt(two_pi) * norm_cdf(-b / a)
            bvn -= (
                math.exp(-hk / 2.0)
                * sp
                * b
                * (1.0 - c * bs * (1.0 - d * bs / 5.0) / 3.0)
            )
        a /= 2.0
        for i in range(len(x)):
            for sign in (-1.0, 1.0):
                xs = (a + sign * a * x[i]) * (a + sign * a * x[i])
                rs = math.sqrt(1.0 - xs)
                asr = -(bs / xs + hk) / 2.0
                if asr > -100.0:
                    sp = 1.0 + c * xs * (1.0 + d * xs)
                    ep = (
                        math.exp(-hk * xs / (2.0 * (1.0 + rs) * (1.0 + rs)))
                        / rs
                    )
                    bvn += a * w[i] * math.exp(asr) * (ep - sp)
        bvn = -bvn / two_pi
    if r > 0.0:
        return bvn + norm_cdf(-max(h, k))
    if h >= k:
        return -bvn
    lo = norm_cdf(k) - norm_cdf(h) if h < 0.0 else norm_cdf(-h) - norm_cdf(-k)
    return lo - bvn


def bivariate_norm_cdf(a: float, b: float, rho: float) -> float:
    """M(a, b; rho) = P(X <= a, Y <= b) (Genz's BVND, ~1e-15)."""
    return min(max(_upper_bivariate(-a, -b, rho), 0.0), 1.0)
