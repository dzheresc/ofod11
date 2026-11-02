// Copyright 2026 Denis Zhereshchin

#include "ofod/credit/credit_risk.h"

#include <cmath>
#include <optional>

#include "ofod/math/normal.h"

namespace ofod::credit {
namespace {

struct MertonResidual {
  double equity;  // Model E0 - observed.
  double vol;     // Model sigma_E E0 - observed.
};

MertonResidual Residual(double v, double sv, double equity, double equity_vol,
                        double debt, double rate, double t) {
  const double sqrt_t = std::sqrt(t);
  const double d1 =
      (std::log(v / debt) + (rate + 0.5 * sv * sv) * t) / (sv * sqrt_t);
  const double d2 = d1 - sv * sqrt_t;
  const double e =
      v * math::NormCdf(d1) - debt * std::exp(-rate * t) * math::NormCdf(d2);
  return {.equity = e - equity,
          .vol = math::NormCdf(d1) * sv * v - equity_vol * equity};
}

}  // namespace

std::optional<MertonResult> MertonSolve(double equity, double equity_vol,
                                        double debt, double rate, double t) {
  if (!(equity > 0.0 && equity_vol > 0.0 && debt > 0.0 && t > 0.0)) {
    return std::nullopt;
  }
  double v = equity + debt * std::exp(-rate * t);
  double sv = equity_vol * equity / v;
  constexpr int kMaxIterations = 100;
  for (int it = 0; it < kMaxIterations; ++it) {
    const MertonResidual f = Residual(v, sv, equity, equity_vol, debt, rate, t);
    if (std::abs(f.equity) < 1e-12 * equity &&
        std::abs(f.vol) < 1e-12 * equity * equity_vol) {
      const double d2 = (std::log(v / debt) + (rate - 0.5 * sv * sv) * t) /
                        (sv * std::sqrt(t));
      return MertonResult{.asset_value = v,
                          .asset_volatility = sv,
                          .default_probability = math::NormCdf(-d2),
                          .debt_value = v - equity};
    }
    // Central-difference Jacobian.
    const double hv = 1e-6 * v;
    const double hs = 1e-6 * sv;
    const MertonResidual fv1 =
        Residual(v + hv, sv, equity, equity_vol, debt, rate, t);
    const MertonResidual fv0 =
        Residual(v - hv, sv, equity, equity_vol, debt, rate, t);
    const MertonResidual fs1 =
        Residual(v, sv + hs, equity, equity_vol, debt, rate, t);
    const MertonResidual fs0 =
        Residual(v, sv - hs, equity, equity_vol, debt, rate, t);
    const double j11 = (fv1.equity - fv0.equity) / (2.0 * hv);
    const double j12 = (fs1.equity - fs0.equity) / (2.0 * hs);
    const double j21 = (fv1.vol - fv0.vol) / (2.0 * hv);
    const double j22 = (fs1.vol - fs0.vol) / (2.0 * hs);
    const double det = j11 * j22 - j12 * j21;
    if (det == 0.0 || !std::isfinite(det)) return std::nullopt;
    const double dv = (f.equity * j22 - f.vol * j12) / det;
    const double ds = (j11 * f.vol - j21 * f.equity) / det;
    v = std::max(v - dv, 0.5 * v);
    sv = std::max(sv - ds, 0.5 * sv);
  }
  return std::nullopt;
}

double JointDefaultProbability(double q1, double q2, double rho) {
  return math::BivariateNormCdf(math::NormInv(q1), math::NormInv(q2), rho);
}

double ConditionalDefaultProbability(double q, double rho, double factor) {
  return math::NormCdf((math::NormInv(q) - std::sqrt(rho) * factor) /
                       std::sqrt(1.0 - rho));
}

double Wcdr(double pd, double rho, double confidence) {
  return math::NormCdf(
      (math::NormInv(pd) + std::sqrt(rho) * math::NormInv(confidence)) /
      std::sqrt(1.0 - rho));
}

double VasicekCreditLoss(double exposure, double pd, double lgd, double rho,
                         double confidence) {
  return exposure * lgd * Wcdr(pd, rho, confidence);
}

}  // namespace ofod::credit
