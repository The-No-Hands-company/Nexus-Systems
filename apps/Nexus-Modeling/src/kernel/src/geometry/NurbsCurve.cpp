#include <nexus/geometry/NurbsCurve.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEpsilon = 1e-10f;

} // namespace

int32_t NurbsCurve::findSpan(float u) const noexcept {
    int32_t n = m_ctlCount;
    if (static_cast<int32_t>(m_knots.size()) <= m_degree) return m_degree;
    if (u >= m_knots[n]) return n - 1;
    if (u <= m_knots[m_degree]) return m_degree;
    int32_t low  = m_degree;
    int32_t high = n;
    int32_t mid  = (low + high) / 2;
    while (u < m_knots[mid] || u >= m_knots[mid + 1]) {
        if (u < m_knots[mid]) high = mid;
        else                   low  = mid;
        mid = (low + high) / 2;
    }
    return mid;
}

void NurbsCurve::basisFuns(int32_t span, float u, std::span<float> out) const noexcept {
    int32_t p = m_degree;
    out[0] = 1.f;
    std::vector<float> left(static_cast<size_t>(p + 1));
    std::vector<float> right(static_cast<size_t>(p + 1));
    for (int32_t j = 1; j <= p; ++j) {
        left[j]  = u - m_knots[span + 1 - j];
        right[j] = m_knots[span + j] - u;
        float saved = 0.f;
        for (int32_t r = 0; r < j; ++r) {
            float tmp = out[r] / (right[r + 1] + left[j - r]);
            out[r] = saved + right[r + 1] * tmp;
            saved  = left[j - r] * tmp;
        }
        out[j] = saved;
    }
}

Vec3 NurbsCurve::evaluate(float u) const noexcept {
    if (!isValid()) return Vec3{};
    int32_t span = findSpan(u);
    std::vector<float> N(static_cast<size_t>(m_degree + 1));
    basisFuns(span, u, N);
    if (isRational()) {
        Vec3  sum{};
        float wSum = 0.f;
        for (int32_t i = 0; i <= m_degree; ++i) {
            int32_t idx = span - m_degree + i;
            float wB = N[i] * m_weights[idx];
            sum.x += wB * m_ctlPts[idx].x;
            sum.y += wB * m_ctlPts[idx].y;
            sum.z += wB * m_ctlPts[idx].z;
            wSum  += wB;
        }
        if (wSum > kEpsilon) {
            float inv = 1.f / wSum;
            return {sum.x * inv, sum.y * inv, sum.z * inv};
        }
        return sum;
    }
    Vec3 pt{};
    for (int32_t i = 0; i <= m_degree; ++i) {
        int32_t idx = span - m_degree + i;
        pt.x += N[i] * m_ctlPts[idx].x;
        pt.y += N[i] * m_ctlPts[idx].y;
        pt.z += N[i] * m_ctlPts[idx].z;
    }
    return pt;
}

Vec3 NurbsCurve::derivative(float u, int32_t order) const noexcept {
    if (!isValid() || order < 0) return Vec3{};
    if (order == 0) return evaluate(u);

    const int32_t p = m_degree;
    if (order > p) return Vec3{};  // derivatives above the degree are identically zero

    const int32_t span = findSpan(u);

    // Derivative basis functions ders[k][j] = d^k/du^k of basis N_{span-p+j,p} at u, for
    // k = 0..order, j = 0..p. The NURBS Book, Algorithm A2.3 (DersBasisFuns). The previous
    // implementation differenced the control points but then returned a single derivative
    // control point instead of combining them with the lower-degree basis, so it was wrong
    // for any degree above 1 (a circle's tangent came out ~70 degrees off).
    std::vector<std::vector<float>> ndu(static_cast<size_t>(p + 1),
                                        std::vector<float>(static_cast<size_t>(p + 1)));
    std::vector<float> left(static_cast<size_t>(p + 1)), right(static_cast<size_t>(p + 1));
    ndu[0][0] = 1.f;
    for (int32_t j = 1; j <= p; ++j) {
        left[j]  = u - m_knots[static_cast<size_t>(span + 1 - j)];
        right[j] = m_knots[static_cast<size_t>(span + j)] - u;
        float saved = 0.f;
        for (int32_t r = 0; r < j; ++r) {
            ndu[j][r] = right[r + 1] + left[j - r];  // lower triangle: knot differences
            const float temp = ndu[r][j - 1] / ndu[j][r];
            ndu[r][j] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        ndu[j][j] = saved;
    }

    std::vector<std::vector<float>> ders(static_cast<size_t>(order + 1),
                                         std::vector<float>(static_cast<size_t>(p + 1), 0.f));
    for (int32_t j = 0; j <= p; ++j) ders[0][j] = ndu[j][p];

    std::vector<std::vector<float>> a(2, std::vector<float>(static_cast<size_t>(p + 1)));
    for (int32_t r = 0; r <= p; ++r) {
        int32_t s1 = 0, s2 = 1;
        a[0][0] = 1.f;
        for (int32_t k = 1; k <= order; ++k) {
            float d = 0.f;
            const int32_t rk = r - k, pk = p - k;
            if (r >= k) {
                a[s2][0] = a[s1][0] / ndu[pk + 1][rk];
                d = a[s2][0] * ndu[rk][pk];
            }
            const int32_t j1 = (rk >= -1) ? 1 : -rk;
            const int32_t j2 = (r - 1 <= pk) ? (k - 1) : (p - r);
            for (int32_t j = j1; j <= j2; ++j) {
                a[s2][j] = (a[s1][j] - a[s1][j - 1]) / ndu[pk + 1][rk + j];
                d += a[s2][j] * ndu[rk + j][pk];
            }
            if (r <= pk) {
                a[s2][k] = -a[s1][k - 1] / ndu[pk + 1][r];
                d += a[s2][k] * ndu[r][pk];
            }
            ders[k][r] = d;
            std::swap(s1, s2);
        }
    }
    // Multiply through by the factorial factors p!/(p-k)!.
    float fac = static_cast<float>(p);
    for (int32_t k = 1; k <= order; ++k) {
        for (int32_t j = 0; j <= p; ++j) ders[k][j] *= fac;
        fac *= static_cast<float>(p - k);
    }

    const int32_t base = span - p;
    if (!isRational()) {
        Vec3 ck{};
        for (int32_t j = 0; j <= p; ++j) {
            const Vec3& P = m_ctlPts[static_cast<size_t>(base + j)];
            ck.x += ders[order][j] * P.x;
            ck.y += ders[order][j] * P.y;
            ck.z += ders[order][j] * P.z;
        }
        return ck;
    }

    // Rational: differentiate the homogeneous curve, then apply the quotient rule — The
    // NURBS Book, Algorithm A4.2 (RatCurveDerivs). Aders[k] is the k-th derivative of the
    // weighted-point numerator, wders[k] of the weight denominator.
    std::vector<Vec3>  Aders(static_cast<size_t>(order + 1), Vec3{});
    std::vector<float> wders(static_cast<size_t>(order + 1), 0.f);
    for (int32_t k = 0; k <= order; ++k) {
        for (int32_t j = 0; j <= p; ++j) {
            const int32_t idx = base + j;
            const float w = m_weights[static_cast<size_t>(idx)];
            const Vec3& P = m_ctlPts[static_cast<size_t>(idx)];
            Aders[k].x += ders[k][j] * (P.x * w);
            Aders[k].y += ders[k][j] * (P.y * w);
            Aders[k].z += ders[k][j] * (P.z * w);
            wders[k]   += ders[k][j] * w;
        }
    }
    if (std::abs(wders[0]) <= kEpsilon) return Vec3{};

    // Binomial coefficients for the quotient rule.
    std::vector<std::vector<float>> bin(static_cast<size_t>(order + 1),
                                        std::vector<float>(static_cast<size_t>(order + 1), 0.f));
    for (int32_t i = 0; i <= order; ++i) {
        bin[i][0] = 1.f;
        for (int32_t j = 1; j <= i; ++j) bin[i][j] = bin[i - 1][j - 1] + bin[i - 1][j];
    }

    std::vector<Vec3> CK(static_cast<size_t>(order + 1), Vec3{});
    for (int32_t k = 0; k <= order; ++k) {
        Vec3 v = Aders[k];
        for (int32_t i = 1; i <= k; ++i) {
            const float b = bin[k][i] * wders[i];
            v.x -= b * CK[k - i].x;
            v.y -= b * CK[k - i].y;
            v.z -= b * CK[k - i].z;
        }
        CK[k] = {v.x / wders[0], v.y / wders[0], v.z / wders[0]};
    }
    return CK[order];
}

NurbsCurve NurbsCurve::insertKnot(float u, int32_t r) const {
    if (r < 1 || !isValid()) return *this;
    int32_t p  = m_degree;
    int32_t n  = m_ctlCount;
    int32_t m  = n + p + 1;
    int32_t k  = findSpan(u);
    int32_t s  = 0;
    for (int32_t i = k; i >= 0 && std::abs(m_knots[i] - u) < kEpsilon; --i) ++s;
    s = std::min(s, p);
    if (s == p) return *this;

    std::vector<float> newKnots;
    newKnots.reserve(static_cast<size_t>(m + r));
    for (int32_t i = 0; i <= k; ++i) newKnots.push_back(m_knots[i]);
    for (int32_t i = 0; i < r;  ++i) newKnots.push_back(u);
    for (int32_t i = k + 1; i < m; ++i) newKnots.push_back(m_knots[i]);

    std::vector<Vec3> newPts(static_cast<size_t>(n + r));
    for (int32_t i = 0; i <= k - p; ++i) newPts[i] = m_ctlPts[i];
    for (int32_t i = k - s; i < n; ++i) newPts[i + r] = m_ctlPts[i];

    std::vector<Vec3> Rw;
    for (int32_t i = 0; i <= p - s; ++i) Rw.push_back(m_ctlPts[k - p + i]);

    for (int32_t j = 1; j <= r; ++j) {
        int32_t L = k - p + j;
        std::vector<Vec3> nextRw(static_cast<size_t>(p - s - j + 1));
        for (int32_t i = 0; i <= p - s - j; ++i) {
            float denom = m_knots[i + k + 1] - m_knots[L + i];
            float alpha = (std::abs(denom) > kEpsilon)
                              ? (u - m_knots[L + i]) / denom
                              : 0.f;
            nextRw[i].x = alpha * Rw[i + 1].x + (1.f - alpha) * Rw[i].x;
            nextRw[i].y = alpha * Rw[i + 1].y + (1.f - alpha) * Rw[i].y;
            nextRw[i].z = alpha * Rw[i + 1].z + (1.f - alpha) * Rw[i].z;
        }
        newPts[L] = nextRw[0];
        if (j < r) newPts[k + r - j - s] = nextRw[p - s - j];
        Rw = std::move(nextRw);
    }
    for (int32_t i = 1; i <= p - s - r; ++i) newPts[k - p + r + i] = Rw[i];

    bool rational = isRational();
    NurbsCurve result(m_degree, std::move(newKnots), std::move(newPts));
    if (rational) {
        std::vector<float> newWeights(static_cast<size_t>(n + r));
        for (int32_t i = 0; i <= k - p; ++i) newWeights[i] = m_weights[i];
        for (int32_t i = k - s; i < n; ++i) newWeights[i + r] = m_weights[i];

        std::vector<float> WRw;
        for (int32_t i = 0; i <= p - s; ++i) WRw.push_back(m_weights[k - p + i]);
        for (int32_t j = 1; j <= r; ++j) {
            int32_t L = k - p + j;
            std::vector<float> nextWRw(static_cast<size_t>(p - s - j + 1));
            for (int32_t i = 0; i <= p - s - j; ++i) {
                float denom = m_knots[i + k + 1] - m_knots[L + i];
                float alpha = (std::abs(denom) > kEpsilon)
                                  ? (u - m_knots[L + i]) / denom
                                  : 0.f;
                nextWRw[i] = alpha * WRw[i + 1] + (1.f - alpha) * WRw[i];
            }
            newWeights[L] = nextWRw[0];
            if (j < r) newWeights[k + r - j - s] = nextWRw[p - s - j];
            WRw = std::move(nextWRw);
        }
        for (int32_t i = 1; i <= p - s - r; ++i) newWeights[k - p + r + i] = WRw[i];
        result.setWeights(std::move(newWeights));
    }
    return result;
}

} // namespace nexus::geometry
