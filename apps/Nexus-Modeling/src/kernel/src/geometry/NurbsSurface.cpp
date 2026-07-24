#include <nexus/geometry/NurbsSurface.h>
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

void basisFunsInternal(int32_t p, const std::vector<float>& knots,
                       int32_t span, float t, std::span<float> out) {
    out[0] = 1.f;
    std::vector<float> left(static_cast<size_t>(p + 1));
    std::vector<float> right(static_cast<size_t>(p + 1));
    for (int32_t j = 1; j <= p; ++j) {
        left[j]  = t - knots[span + 1 - j];
        right[j] = knots[span + j] - t;
        float saved = 0.f;
        for (int32_t r = 0; r < j; ++r) {
            float tmp  = out[r] / (right[r + 1] + left[j - r]);
            out[r] = saved + right[r + 1] * tmp;
            saved  = left[j - r] * tmp;
        }
        out[j] = saved;
    }
}

int32_t findSpanInternal(int32_t n, int32_t p, const std::vector<float>& knots,
                         float t) {
    if (t >= knots[n]) return n - 1;
    if (t <= knots[p]) return p;
    int32_t low = p, high = n, mid = (low + high) / 2;
    while (t < knots[mid] || t >= knots[mid + 1]) {
        if (t < knots[mid]) high = mid; else low = mid;
        mid = (low + high) / 2;
    }
    return mid;
}

// Derivative basis functions ders[k][j] = d^k/dt^k of basis N_{span-p+j,p}(t), for
// k = 0..order, j = 0..p. The NURBS Book, Algorithm A2.3 (DersBasisFuns). ders[0][.] is
// exactly the plain basis-function row, so a single call gives both the values and their
// derivatives.
void dersBasisFuns(int32_t p, const std::vector<float>& knots, int32_t span, float t,
                   int32_t order, std::vector<std::vector<float>>& ders) {
    std::vector<std::vector<float>> ndu(static_cast<size_t>(p + 1),
                                        std::vector<float>(static_cast<size_t>(p + 1)));
    std::vector<float> left(static_cast<size_t>(p + 1)), right(static_cast<size_t>(p + 1));
    ndu[0][0] = 1.f;
    for (int32_t j = 1; j <= p; ++j) {
        left[j]  = t - knots[static_cast<size_t>(span + 1 - j)];
        right[j] = knots[static_cast<size_t>(span + j)] - t;
        float saved = 0.f;
        for (int32_t r = 0; r < j; ++r) {
            ndu[j][r] = right[r + 1] + left[j - r];  // lower triangle: knot differences
            const float temp = ndu[r][j - 1] / ndu[j][r];
            ndu[r][j] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        ndu[j][j] = saved;
    }

    ders.assign(static_cast<size_t>(order + 1),
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
    float fac = static_cast<float>(p);
    for (int32_t k = 1; k <= order; ++k) {
        for (int32_t j = 0; j <= p; ++j) ders[k][j] *= fac;
        fac *= static_cast<float>(p - k);
    }
}

} // namespace

int32_t NurbsSurface::findSpanU(float u) const noexcept {
    return findSpanInternal(m_nU, m_degU, m_knotU, u);
}

int32_t NurbsSurface::findSpanV(float v) const noexcept {
    return findSpanInternal(m_nV, m_degV, m_knotV, v);
}

Vec3 NurbsSurface::evaluate(float u, float v) const noexcept {
    if (!isValid()) return Vec3{};
    int32_t su = findSpanU(u);
    int32_t sv = findSpanV(v);
    int32_t pu = m_degU, pv = m_degV;
    std::vector<float> Nu(static_cast<size_t>(pu + 1));
    std::vector<float> Nv(static_cast<size_t>(pv + 1));
    basisFunsInternal(pu, m_knotU, su, u, Nu);
    basisFunsInternal(pv, m_knotV, sv, v, Nv);

    if (isRational()) {
        Vec3  sum{};
        float wSum = 0.f;
        for (int32_t i = 0; i <= pu; ++i) {
            int32_t ci = su - pu + i;
            for (int32_t j = 0; j <= pv; ++j) {
                int32_t cj  = sv - pv + j;
                size_t  idx = static_cast<size_t>(ci * m_nV + cj);
                float wB = Nu[i] * Nv[j] * m_weights[idx];
                sum.x += wB * m_ctlPts[idx].x;
                sum.y += wB * m_ctlPts[idx].y;
                sum.z += wB * m_ctlPts[idx].z;
                wSum  += wB;
            }
        }
        if (wSum > kEpsilon) {
            float inv = 1.f / wSum;
            return {sum.x * inv, sum.y * inv, sum.z * inv};
        }
        return sum;
    }

    Vec3 pt{};
    for (int32_t i = 0; i <= pu; ++i) {
        int32_t ci = su - pu + i;
        for (int32_t j = 0; j <= pv; ++j) {
            int32_t cj  = sv - pv + j;
            float bf = Nu[i] * Nv[j];
            size_t idx = static_cast<size_t>(ci * m_nV + cj);
            pt.x += bf * m_ctlPts[idx].x;
            pt.y += bf * m_ctlPts[idx].y;
            pt.z += bf * m_ctlPts[idx].z;
        }
    }
    return pt;
}

// First partial dS/du. The previous implementation summed the u-direction difference
// control points weighted only by the v-basis, omitting the degree-(pu-1) basis functions
// N_{i,pu-1}(u) that must weight each one — the same defect the curve derivative had. That
// is correct only when pu == 1; for any higher u-degree it summed the derivative control
// points with weight 1 instead of their basis values.
//
// Correct form (The NURBS Book, tensor-product surface derivative): take the first-order
// derivative basis in u (A2.3) and the plain basis in v, combine over the control net; for
// a rational surface apply the first-order quotient rule S_u = (A_u - w_u S) / w.
Vec3 NurbsSurface::derivativeU(float u, float v) const noexcept {
    if (!isValid()) return Vec3{};
    const int32_t pu = m_degU, pv = m_degV;
    const int32_t su = findSpanU(u), sv = findSpanV(v);

    std::vector<std::vector<float>> dNu;  // dNu[0][i] = value, dNu[1][i] = d/du
    dersBasisFuns(pu, m_knotU, su, u, 1, dNu);
    std::vector<float> Nv(static_cast<size_t>(pv + 1));
    basisFunsInternal(pv, m_knotV, sv, v, Nv);

    const int32_t baseU = su - pu, baseV = sv - pv;

    if (!isRational()) {
        Vec3 d{};
        for (int32_t i = 0; i <= pu; ++i) {
            for (int32_t j = 0; j <= pv; ++j) {
                const float b = dNu[1][i] * Nv[j];
                const Vec3& P = m_ctlPts[static_cast<size_t>((baseU + i) * m_nV + (baseV + j))];
                d.x += b * P.x; d.y += b * P.y; d.z += b * P.z;
            }
        }
        return d;
    }

    Vec3 A{}, Au{};
    float w = 0.f, wu = 0.f;
    for (int32_t i = 0; i <= pu; ++i) {
        for (int32_t j = 0; j <= pv; ++j) {
            const size_t idx = static_cast<size_t>((baseU + i) * m_nV + (baseV + j));
            const float wt = m_weights[idx];
            const Vec3& P = m_ctlPts[idx];
            const float b0 = dNu[0][i] * Nv[j];
            const float b1 = dNu[1][i] * Nv[j];
            A.x  += b0 * wt * P.x; A.y  += b0 * wt * P.y; A.z  += b0 * wt * P.z;
            w    += b0 * wt;
            Au.x += b1 * wt * P.x; Au.y += b1 * wt * P.y; Au.z += b1 * wt * P.z;
            wu   += b1 * wt;
        }
    }
    if (std::abs(w) <= kEpsilon) return Vec3{};
    const float invW = 1.f / w;
    const Vec3 S{A.x * invW, A.y * invW, A.z * invW};
    return {(Au.x - wu * S.x) * invW,
            (Au.y - wu * S.y) * invW,
            (Au.z - wu * S.z) * invW};
}

// First partial dS/dv — mirror of derivativeU with the derivative basis taken in v.
Vec3 NurbsSurface::derivativeV(float u, float v) const noexcept {
    if (!isValid()) return Vec3{};
    const int32_t pu = m_degU, pv = m_degV;
    const int32_t su = findSpanU(u), sv = findSpanV(v);

    std::vector<float> Nu(static_cast<size_t>(pu + 1));
    basisFunsInternal(pu, m_knotU, su, u, Nu);
    std::vector<std::vector<float>> dNv;  // dNv[0][j] = value, dNv[1][j] = d/dv
    dersBasisFuns(pv, m_knotV, sv, v, 1, dNv);

    const int32_t baseU = su - pu, baseV = sv - pv;

    if (!isRational()) {
        Vec3 d{};
        for (int32_t i = 0; i <= pu; ++i) {
            for (int32_t j = 0; j <= pv; ++j) {
                const float b = Nu[i] * dNv[1][j];
                const Vec3& P = m_ctlPts[static_cast<size_t>((baseU + i) * m_nV + (baseV + j))];
                d.x += b * P.x; d.y += b * P.y; d.z += b * P.z;
            }
        }
        return d;
    }

    Vec3 A{}, Av{};
    float w = 0.f, wv = 0.f;
    for (int32_t i = 0; i <= pu; ++i) {
        for (int32_t j = 0; j <= pv; ++j) {
            const size_t idx = static_cast<size_t>((baseU + i) * m_nV + (baseV + j));
            const float wt = m_weights[idx];
            const Vec3& P = m_ctlPts[idx];
            const float b0 = Nu[i] * dNv[0][j];
            const float b1 = Nu[i] * dNv[1][j];
            A.x  += b0 * wt * P.x; A.y  += b0 * wt * P.y; A.z  += b0 * wt * P.z;
            w    += b0 * wt;
            Av.x += b1 * wt * P.x; Av.y += b1 * wt * P.y; Av.z += b1 * wt * P.z;
            wv   += b1 * wt;
        }
    }
    if (std::abs(w) <= kEpsilon) return Vec3{};
    const float invW = 1.f / w;
    const Vec3 S{A.x * invW, A.y * invW, A.z * invW};
    return {(Av.x - wv * S.x) * invW,
            (Av.y - wv * S.y) * invW,
            (Av.z - wv * S.z) * invW};
}

NurbsSurface NurbsSurface::insertKnotU(float u, int32_t r) const {
    if (r < 1 || !isValid()) return *this;
    int32_t pu = m_degU, pv = m_degV;
    int32_t nu = m_nU, nv = m_nV;
    int32_t mu = nu + pu + 1;
    int32_t k  = findSpanU(u);
    int32_t s  = 0;
    for (int32_t i = k; i >= 0 && std::abs(m_knotU[i] - u) < 1e-10f; --i) ++s;
    s = std::min(s, pu);
    if (s == pu) return *this;

    std::vector<float> newKnotU;
    newKnotU.reserve(static_cast<size_t>(mu + r));
    for (int32_t i = 0; i <= k; ++i) newKnotU.push_back(m_knotU[i]);
    for (int32_t i = 0; i < r;  ++i) newKnotU.push_back(u);
    for (int32_t i = k + 1; i < mu; ++i) newKnotU.push_back(m_knotU[i]);

    int32_t newNu = nu + r;
    std::vector<Vec3> newPts(static_cast<size_t>(newNu * nv));

    for (int32_t row = 0; row < nv; ++row) {
        std::vector<Vec3> colPts(static_cast<size_t>(nu));
        for (int32_t ci = 0; ci < nu; ++ci)
            colPts[ci] = m_ctlPts[static_cast<size_t>(ci * nv + row)];

        std::vector<Vec3> newColPts(static_cast<size_t>(newNu));
        for (int32_t i = 0; i <= k - pu; ++i) newColPts[i] = colPts[i];
        for (int32_t i = k - s; i < nu; ++i) newColPts[i + r] = colPts[i];

        std::vector<Vec3> Rw;
        for (int32_t i = 0; i <= pu - s; ++i) Rw.push_back(colPts[k - pu + i]);

        for (int32_t j = 1; j <= r; ++j) {
            int32_t L = k - pu + j;
            std::vector<Vec3> nextRw(static_cast<size_t>(pu - s - j + 1));
            for (int32_t i = 0; i <= pu - s - j; ++i) {
                float denom = m_knotU[i + k + 1] - m_knotU[L + i];
                float alpha = (std::abs(denom) > 1e-10f)
                                  ? (u - m_knotU[L + i]) / denom
                                  : 0.f;
                nextRw[i].x = alpha * Rw[i + 1].x + (1.f - alpha) * Rw[i].x;
                nextRw[i].y = alpha * Rw[i + 1].y + (1.f - alpha) * Rw[i].y;
                nextRw[i].z = alpha * Rw[i + 1].z + (1.f - alpha) * Rw[i].z;
            }
            newColPts[L] = nextRw[0];
            if (j < r) newColPts[k + r - j - s] = nextRw[pu - s - j];
            Rw = std::move(nextRw);
        }
        for (int32_t i = 1; i <= pu - s - r; ++i) newColPts[k - pu + r + i] = Rw[i];

        for (int32_t ci = 0; ci < newNu; ++ci)
            newPts[static_cast<size_t>(ci * nv + row)] = newColPts[ci];
    }

    if (isRational()) {
        std::vector<float> newWts(static_cast<size_t>(newNu * nv));
        for (int32_t row = 0; row < nv; ++row) {
            std::vector<float> colWt(static_cast<size_t>(nu));
            for (int32_t ci = 0; ci < nu; ++ci)
                colWt[ci] = m_weights[static_cast<size_t>(ci * nv + row)];

            std::vector<float> newColWt(static_cast<size_t>(newNu));
            for (int32_t i = 0; i <= k - pu; ++i) newColWt[i] = colWt[i];
            for (int32_t i = k - s; i < nu; ++i) newColWt[i + r] = colWt[i];

            std::vector<float> Rw;
            for (int32_t i = 0; i <= pu - s; ++i) Rw.push_back(colWt[k - pu + i]);
            for (int32_t j = 1; j <= r; ++j) {
                int32_t L = k - pu + j;
                std::vector<float> nextRw(static_cast<size_t>(pu - s - j + 1));
                for (int32_t i = 0; i <= pu - s - j; ++i) {
                    float denom = m_knotU[i + k + 1] - m_knotU[L + i];
                    float alpha = (std::abs(denom) > 1e-10f)
                                      ? (u - m_knotU[L + i]) / denom
                                      : 0.f;
                    nextRw[i] = alpha * Rw[i + 1] + (1.f - alpha) * Rw[i];
                }
                newColWt[L] = nextRw[0];
                if (j < r) newColWt[k + r - j - s] = nextRw[pu - s - j];
                Rw = std::move(nextRw);
            }
            for (int32_t i = 1; i <= pu - s - r; ++i) newColWt[k - pu + r + i] = Rw[i];
            for (int32_t ci = 0; ci < newNu; ++ci)
                newWts[static_cast<size_t>(ci * nv + row)] = newColWt[ci];
        }
        return NurbsSurface(pu, pv, std::move(newKnotU), m_knotV,
                            std::move(newPts), newNu, nv, std::move(newWts));
    }

    return NurbsSurface(pu, pv, std::move(newKnotU), m_knotV,
                        std::move(newPts), newNu, nv);
}

NurbsSurface NurbsSurface::insertKnotV(float v, int32_t r) const {
    if (r < 1 || !isValid()) return *this;
    int32_t pu = m_degU, pv = m_degV;
    int32_t nu = m_nU, nv = m_nV;
    int32_t mv = nv + pv + 1;
    int32_t k  = findSpanV(v);
    int32_t s  = 0;
    for (int32_t i = k; i >= 0 && std::abs(m_knotV[i] - v) < 1e-10f; --i) ++s;
    s = std::min(s, pv);
    if (s == pv) return *this;

    std::vector<float> newKnotV;
    newKnotV.reserve(static_cast<size_t>(mv + r));
    for (int32_t i = 0; i <= k; ++i) newKnotV.push_back(m_knotV[i]);
    for (int32_t i = 0; i < r;  ++i) newKnotV.push_back(v);
    for (int32_t i = k + 1; i < mv; ++i) newKnotV.push_back(m_knotV[i]);

    int32_t newNv = nv + r;
    std::vector<Vec3> newPts(static_cast<size_t>(nu * newNv));

    for (int32_t col = 0; col < nu; ++col) {
        std::vector<Vec3> rowPts(static_cast<size_t>(nv));
        for (int32_t ri = 0; ri < nv; ++ri)
            rowPts[ri] = m_ctlPts[static_cast<size_t>(col * nv + ri)];

        std::vector<Vec3> newRowPts(static_cast<size_t>(newNv));
        for (int32_t i = 0; i <= k - pv; ++i) newRowPts[i] = rowPts[i];
        for (int32_t i = k - s; i < nv; ++i) newRowPts[i + r] = rowPts[i];

        std::vector<Vec3> Rw;
        for (int32_t i = 0; i <= pv - s; ++i) Rw.push_back(rowPts[k - pv + i]);

        for (int32_t j = 1; j <= r; ++j) {
            int32_t L = k - pv + j;
            std::vector<Vec3> nextRw(static_cast<size_t>(pv - s - j + 1));
            for (int32_t i = 0; i <= pv - s - j; ++i) {
                float denom = m_knotV[i + k + 1] - m_knotV[L + i];
                float alpha = (std::abs(denom) > 1e-10f)
                                  ? (v - m_knotV[L + i]) / denom
                                  : 0.f;
                nextRw[i].x = alpha * Rw[i + 1].x + (1.f - alpha) * Rw[i].x;
                nextRw[i].y = alpha * Rw[i + 1].y + (1.f - alpha) * Rw[i].y;
                nextRw[i].z = alpha * Rw[i + 1].z + (1.f - alpha) * Rw[i].z;
            }
            newRowPts[L] = nextRw[0];
            if (j < r) newRowPts[k + r - j - s] = nextRw[pv - s - j];
            Rw = std::move(nextRw);
        }
        for (int32_t i = 1; i <= pv - s - r; ++i) newRowPts[k - pv + r + i] = Rw[i];

        for (int32_t ri = 0; ri < newNv; ++ri)
            newPts[static_cast<size_t>(col * newNv + ri)] = newRowPts[ri];
    }

    if (isRational()) {
        std::vector<float> newWts(static_cast<size_t>(nu * newNv));
        for (int32_t col = 0; col < nu; ++col) {
            std::vector<float> rowWt(static_cast<size_t>(nv));
            for (int32_t ri = 0; ri < nv; ++ri)
                rowWt[ri] = m_weights[static_cast<size_t>(col * nv + ri)];
            std::vector<float> newRowWt(static_cast<size_t>(newNv));
            for (int32_t i = 0; i <= k - pv; ++i) newRowWt[i] = rowWt[i];
            for (int32_t i = k - s; i < nv; ++i) newRowWt[i + r] = rowWt[i];
            std::vector<float> Rw;
            for (int32_t i = 0; i <= pv - s; ++i) Rw.push_back(rowWt[k - pv + i]);
            for (int32_t j = 1; j <= r; ++j) {
                int32_t L = k - pv + j;
                std::vector<float> nextRw(static_cast<size_t>(pv - s - j + 1));
                for (int32_t i = 0; i <= pv - s - j; ++i) {
                    float denom = m_knotV[i + k + 1] - m_knotV[L + i];
                    float alpha = (std::abs(denom) > 1e-10f)
                                      ? (v - m_knotV[L + i]) / denom
                                      : 0.f;
                    nextRw[i] = alpha * Rw[i + 1] + (1.f - alpha) * Rw[i];
                }
                newRowWt[L] = nextRw[0];
                if (j < r) newRowWt[k + r - j - s] = nextRw[pv - s - j];
                Rw = std::move(nextRw);
            }
            for (int32_t i = 1; i <= pv - s - r; ++i) newRowWt[k - pv + r + i] = Rw[i];
            for (int32_t ri = 0; ri < newNv; ++ri)
                newWts[static_cast<size_t>(col * newNv + ri)] = newRowWt[ri];
        }
        return NurbsSurface(pu, pv, m_knotU, std::move(newKnotV),
                            std::move(newPts), nu, newNv, std::move(newWts));
    }

    return NurbsSurface(pu, pv, m_knotU, std::move(newKnotV),
                        std::move(newPts), nu, newNv);
}

} // namespace nexus::geometry
