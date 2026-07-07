#pragma once

#include <vector>
#include <array>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Discrete differential geometry operators on triangle meshes.
///
/// Estimates mean and Gaussian curvature per vertex using the cotangent
/// (Laplace-Beltrami) operator together with the mixed Voronoi area of Meyer et
/// al. Principal curvatures follow from the mean/Gaussian pair, and principal
/// directions are recovered from Taubin's curvature-tensor eigenanalysis in the
/// vertex tangent plane.
class SurfaceCurvature {
public:
    using Vector3 = nexus::utility::math::Vector3;

    struct PrincipalCurvature {
        float k1 = 0.0f;      // larger principal curvature
        float k2 = 0.0f;      // smaller principal curvature
        Vector3 dir1;         // direction of k1 (unit, tangent)
        Vector3 dir2;         // direction of k2 (unit, tangent)
    };

    struct Curvature {
        float mean = 0.0f;
        float gaussian = 0.0f;
    };

    /// @brief Cotangent of an angle.
    static float Cotan(float angle) {
        float s = std::sin(angle);
        if (std::abs(s) < 1e-8f) return 0.0f;
        return std::cos(angle) / s;
    }

    /// @brief Signed mean curvature at a vertex (cotangent Laplacian / 2).
    static float computeMeanCurvature(const std::vector<Vector3>& vertices,
                                      const std::vector<unsigned>& indices,
                                      int vertexIndex) {
        auto tris = incidentTriangles(indices, vertexIndex);
        if (tris.empty()) return 0.0f;

        std::unordered_map<int, float> cotWeight;
        float mixedArea = 0.0f;
        accumulate(vertices, tris, vertexIndex, cotWeight, mixedArea);
        if (mixedArea < 1e-12f) return 0.0f;

        const Vector3& xi = vertices[vertexIndex];
        Vector3 K(0.0f, 0.0f, 0.0f);
        for (const auto& kv : cotWeight)
            K += (xi - vertices[kv.first]) * kv.second;
        K = K / (2.0f * mixedArea);

        Vector3 n = vertexNormal(vertices, tris, vertexIndex);
        return 0.5f * dot(K, n);
    }

    /// @brief Gaussian curvature at a vertex via the angle deficit.
    static float computeGaussianCurvature(const std::vector<Vector3>& vertices,
                                          const std::vector<unsigned>& indices,
                                          int vertexIndex) {
        auto tris = incidentTriangles(indices, vertexIndex);
        if (tris.empty()) return 0.0f;

        std::unordered_map<int, float> cotWeight;
        float mixedArea = 0.0f;
        accumulate(vertices, tris, vertexIndex, cotWeight, mixedArea);
        if (mixedArea < 1e-12f) return 0.0f;

        const Vector3& xi = vertices[vertexIndex];
        float angleSum = 0.0f;
        for (const auto& t : tris) {
            const Vector3& j = vertices[t[1]];
            const Vector3& k = vertices[t[2]];
            angleSum += angleBetween(j - xi, k - xi);
        }
        const float PI = 3.14159265358979323846f;
        float target = isBoundaryVertex(indices, vertexIndex) ? PI : 2.0f * PI;
        return (target - angleSum) / mixedArea;
    }

    /// @brief Principal curvatures and directions at a vertex.
    static PrincipalCurvature computePrincipalCurvatures(
            const std::vector<Vector3>& vertices,
            const std::vector<unsigned>& indices,
            int vertexIndex) {
        PrincipalCurvature result;
        auto tris = incidentTriangles(indices, vertexIndex);
        if (tris.empty()) return result;

        float H = computeMeanCurvature(vertices, indices, vertexIndex);
        float G = computeGaussianCurvature(vertices, indices, vertexIndex);
        float disc = std::max(H * H - G, 0.0f);
        float root = std::sqrt(disc);
        result.k1 = H + root;
        result.k2 = H - root;

        // Curvature tensor (Taubin) restricted to the tangent plane.
        Vector3 n = vertexNormal(vertices, tris, vertexIndex);
        Vector3 u, v;
        tangentBasis(n, u, v);

        const Vector3& xi = vertices[vertexIndex];
        float Muu = 0.0f, Muv = 0.0f, Mvv = 0.0f;
        float totalW = 0.0f;

        std::unordered_map<int, float> areaW;
        for (const auto& t : tris) {
            float area = triangleArea(xi, vertices[t[1]], vertices[t[2]]);
            areaW[t[1]] += area;
            areaW[t[2]] += area;
        }

        std::unordered_map<int, bool> processed;
        for (const auto& t : tris) {
            for (int idx : {t[1], t[2]}) {
                if (processed[idx]) continue;
                processed[idx] = true;
                Vector3 e = vertices[idx] - xi;
                float len2 = e.lengthSquared();
                if (len2 < 1e-12f) continue;
                float kappa = 2.0f * dot(n, e) / len2;
                Vector3 T = e - n * dot(n, e);
                float tlen = T.length();
                if (tlen < 1e-8f) continue;
                T = T / tlen;
                float w = areaW.count(idx) ? areaW[idx] : 1.0f;
                float tu = dot(T, u);
                float tv = dot(T, v);
                Muu += w * kappa * tu * tu;
                Muv += w * kappa * tu * tv;
                Mvv += w * kappa * tv * tv;
                totalW += w;
            }
        }
        if (totalW > 1e-12f) {
            Muu /= totalW; Muv /= totalW; Mvv /= totalW;
        }

        // 2x2 symmetric eigen-decomposition.
        float e1, e2, cs, sn;
        eigen2x2(Muu, Muv, Mvv, e1, e2, cs, sn);
        // Eigenvector for e1 is (cs, sn) in the (u,v) basis; e2 is orthogonal.
        Vector3 d1 = (u * cs + v * sn).normalized();
        Vector3 d2 = (u * -sn + v * cs).normalized();
        result.dir1 = d1;
        result.dir2 = d2;
        return result;
    }

    /// @brief Mean and Gaussian curvature for every vertex.
    static std::vector<Curvature> computeAllCurvatures(
            const std::vector<Vector3>& vertices,
            const std::vector<unsigned>& indices) {
        std::vector<Curvature> out(vertices.size());
        for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
            out[i].mean = computeMeanCurvature(vertices, indices, i);
            out[i].gaussian = computeGaussianCurvature(vertices, indices, i);
        }
        return out;
    }

private:
    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }
    static float triangleArea(const Vector3& a, const Vector3& b, const Vector3& c) {
        return 0.5f * cross(b - a, c - a).length();
    }
    static float angleBetween(const Vector3& a, const Vector3& b) {
        float la = a.length(), lb = b.length();
        if (la < 1e-8f || lb < 1e-8f) return 0.0f;
        float c = std::clamp(dot(a, b) / (la * lb), -1.0f, 1.0f);
        return std::acos(c);
    }
    // Cotangent of the angle between two vectors (numerically stable form).
    static float cotBetween(const Vector3& a, const Vector3& b) {
        float d = dot(a, b);
        float crossLen = cross(a, b).length();
        if (crossLen < 1e-12f) return 0.0f;
        return d / crossLen;
    }

    // Triangles incident to vertexIndex, each reordered so element [0] is it.
    static std::vector<std::array<int, 3>> incidentTriangles(
            const std::vector<unsigned>& indices, int vi) {
        std::vector<std::array<int, 3>> tris;
        const size_t triCount = indices.size() / 3;
        for (size_t t = 0; t < triCount; ++t) {
            int a = static_cast<int>(indices[t * 3 + 0]);
            int b = static_cast<int>(indices[t * 3 + 1]);
            int c = static_cast<int>(indices[t * 3 + 2]);
            if (a == vi) tris.push_back({a, b, c});
            else if (b == vi) tris.push_back({b, c, a});
            else if (c == vi) tris.push_back({c, a, b});
        }
        return tris;
    }

    // Accumulate cotangent edge weights and the mixed Voronoi area at a vertex.
    static void accumulate(const std::vector<Vector3>& vertices,
                           const std::vector<std::array<int, 3>>& tris,
                           int vi,
                           std::unordered_map<int, float>& cotWeight,
                           float& mixedArea) {
        const Vector3& xi = vertices[vi];
        for (const auto& t : tris) {
            const int jIdx = t[1];
            const int kIdx = t[2];
            const Vector3& xj = vertices[jIdx];
            const Vector3& xk = vertices[kIdx];

            // Angles of triangle (i, j, k).
            float angI = angleBetween(xj - xi, xk - xi);
            float angJ = angleBetween(xi - xj, xk - xj);
            float angK = angleBetween(xi - xk, xj - xk);

            // Edge (i,j) is opposite the angle at k; edge (i,k) opposite j.
            cotWeight[jIdx] += cotBetween(xi - xk, xj - xk); // cot at k
            cotWeight[kIdx] += cotBetween(xi - xj, xk - xj); // cot at j

            // Mixed area contribution (Meyer et al.).
            const float PI_2 = 1.57079632679489661923f;
            if (angI < PI_2 && angJ < PI_2 && angK < PI_2) {
                float lij2 = (xi - xj).lengthSquared();
                float lik2 = (xi - xk).lengthSquared();
                mixedArea += (cotBetween(xi - xk, xj - xk) * lij2 +
                              cotBetween(xi - xj, xk - xj) * lik2) / 8.0f;
            } else {
                float area = triangleArea(xi, xj, xk);
                mixedArea += (angI >= PI_2) ? area * 0.5f : area * 0.25f;
            }
        }
    }

    static Vector3 vertexNormal(const std::vector<Vector3>& vertices,
                                const std::vector<std::array<int, 3>>& tris,
                                int vi) {
        const Vector3& xi = vertices[vi];
        Vector3 n(0.0f, 0.0f, 0.0f);
        for (const auto& t : tris) {
            Vector3 fn = cross(vertices[t[1]] - xi, vertices[t[2]] - xi);
            n += fn; // area-weighted (magnitude proportional to 2*area)
        }
        float len = n.length();
        return len > 1e-8f ? n / len : Vector3(0, 0, 1);
    }

    static bool isBoundaryVertex(const std::vector<unsigned>& indices, int vi) {
        std::unordered_map<int, int> edgeCount;
        const size_t triCount = indices.size() / 3;
        for (size_t t = 0; t < triCount; ++t) {
            int a = static_cast<int>(indices[t * 3 + 0]);
            int b = static_cast<int>(indices[t * 3 + 1]);
            int c = static_cast<int>(indices[t * 3 + 2]);
            int other0 = -1, other1 = -1;
            if (a == vi) { other0 = b; other1 = c; }
            else if (b == vi) { other0 = c; other1 = a; }
            else if (c == vi) { other0 = a; other1 = b; }
            else continue;
            ++edgeCount[other0];
            ++edgeCount[other1];
        }
        for (const auto& kv : edgeCount)
            if (kv.second < 2) return true; // spoke edge used by a single triangle
        return false;
    }

    static void tangentBasis(const Vector3& n, Vector3& u, Vector3& v) {
        Vector3 ref = (std::abs(n.x) < 0.9f) ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
        u = cross(n, ref).normalized();
        v = cross(n, u).normalized();
    }

    // Eigen-decomposition of [[a, b],[b, c]]; e1 >= e2, (cs,sn) is the e1 vector.
    static void eigen2x2(float a, float b, float c,
                         float& e1, float& e2, float& cs, float& sn) {
        float tr = a + c;
        float diff = a - c;
        float disc = std::sqrt(diff * diff + 4.0f * b * b);
        e1 = 0.5f * (tr + disc);
        e2 = 0.5f * (tr - disc);
        if (std::abs(b) > 1e-10f) {
            float y = e1 - c;
            float len = std::sqrt(b * b + y * y);
            cs = b / len;
            sn = y / len;
        } else {
            // Already diagonal: pick axis with the larger eigenvalue.
            if (a >= c) { cs = 1.0f; sn = 0.0f; }
            else { cs = 0.0f; sn = 1.0f; }
        }
    }
};

} // namespace nexus::utility::graphics
