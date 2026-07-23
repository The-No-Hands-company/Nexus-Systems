#include <nexus/geometry/MeshAmbientOcclusion.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/geometry/MeshPointSample.h>
#include "SplitMix64.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

bool rayTriangleIntersect(
    const Vec3& orig, const Vec3& dir,
    const Vec3& v0, const Vec3& v1, const Vec3& v2,
    float& t, float& u, float& v)
{
    Vec3 e1{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    Vec3 e2{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
    Vec3 pvec = dir.cross(e2);
    float det = e1.dot(pvec);
    if (det > -1e-8f && det < 1e-8f) return false;
    float invDet = 1.f / det;
    Vec3 tvec{orig.x - v0.x, orig.y - v0.y, orig.z - v0.z};
    u = tvec.dot(pvec) * invDet;
    if (u < 0.f || u > 1.f) return false;
    Vec3 qvec = tvec.cross(e1);
    v = dir.dot(qvec) * invDet;
    if (v < 0.f || u + v > 1.f) return false;
    t = e2.dot(qvec) * invDet;
    return t > 0.f;
}

Vec3 cosineSampleHemisphere(float u1, float u2, const Vec3& N) {
    float phi = 2.f * 3.14159265358979323846f * u1;
    float cosTheta = std::sqrt(1.f - u2);
    float sinTheta = std::sqrt(u2);

    Vec3 H{std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta};

    Vec3 tangent;
    if (std::fabs(N.x) > 0.9f) {
        tangent = {0.f, N.z, -N.y};
    } else {
        tangent = {N.y, -N.x, 0.f};
    }
    float tLenSq = tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z;
    if (tLenSq < 1e-12f) {
        tangent = {N.z, 0.f, -N.x};
        tLenSq = tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z;
        if (tLenSq < 1e-12f) {
            tangent = {-N.y, N.x, 0.f};
            tLenSq = tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z;
        }
    }
    float invLen = 1.f / std::sqrt(tLenSq);
    tangent.x *= invLen;
    tangent.y *= invLen;
    tangent.z *= invLen;
    Vec3 bitangent = N.cross(tangent);

    return (tangent * H.x + bitangent * H.y + N * H.z).normalize();
}

struct EvalResult {
    float ao;
    Vec3 bentNormal;
};

EvalResult evaluateAtPoint(
    const Vec3& P, const Vec3& N,
    uint32_t sampleCount, float maxDistance,
    internal::SplitMix64& rng,
    const MeshBVH* bvh,
    const Mesh& mesh)
{
    uint32_t hitCount = 0;
    Vec3 unoccludedDirSum{0.f, 0.f, 0.f};

    // Shadow-ray bias. The ray starts ON the surface, so the very first thing it meets is
    // the triangle it started from, at t == 0. The BVH reports only the NEAREST hit, so
    // that self-intersection is all it ever returns and the `t > epsilon` guard below then
    // reads it as "nothing was hit" — every ray came back unoccluded and the whole bake
    // was a uniform 1.0 no matter what stood in the way. Measured on a plane with a sphere
    // resting on it: vertices directly beneath the sphere scored exactly the same as the
    // far rim.
    //
    // Lifting the origin off the surface by a hair removes the self-hit entirely, and the
    // occluder behind it becomes visible: the same ray goes from t == -0 to t == 0.05.
    // The offset is scale-aware — a fraction of the query's own reach, plus a term for the
    // float resolution at this coordinate magnitude — so it behaves the same on a
    // millimetre part as on a kilometre of terrain.
    const float magnitude = std::max({std::abs(P.x), std::abs(P.y), std::abs(P.z), 1.f});
    const float bias = std::max(maxDistance * 1e-4f, magnitude * 1e-5f);
    const Vec3 origin{P.x + N.x * bias, P.y + N.y * bias, P.z + N.z * bias};

    for (uint32_t r = 0; r < sampleCount; ++r) {
        float u1 = static_cast<float>(rng.uniform01());
        float u2 = static_cast<float>(rng.uniform01());
        Vec3 rayDir = cosineSampleHemisphere(u1, u2, N);

        bool hit = false;

        if (bvh) {
            Ray ray{origin, rayDir};
            auto hitResult = bvh->raycast(ray);
            if (hitResult.t < maxDistance && hitResult.t > 1e-6f) {
                hit = true;
            }
        } else {
            const auto& topo = mesh.topology();
            const auto& pos = mesh.attributes().positions();
            float bestT = maxDistance;
            for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
                const Face& face = topo.face(fi);
                if (!face.indicesInBounds(pos.size())) continue;
                if (face.indices.size() < 3) continue;
                const Vec3& a = pos[face.indices[0]];
                for (size_t vi = 1; vi + 1 < face.indices.size(); ++vi) {
                    const Vec3& b = pos[face.indices[vi]];
                    const Vec3& c = pos[face.indices[vi + 1]];
                    float t, u, v;
                    if (rayTriangleIntersect(origin, rayDir, a, b, c, t, u, v)) {
                        if (t > 1e-6f && t < bestT) {
                            bestT = t;
                            hit = true;
                        }
                    }
                }
            }
        }

        if (hit) {
            ++hitCount;
        } else {
            unoccludedDirSum = unoccludedDirSum + rayDir;
        }
    }

    float ao = 1.f - static_cast<float>(hitCount) / static_cast<float>(sampleCount);

    Vec3 bentNormal;
    float sumLenSq = unoccludedDirSum.lengthSq();
    if (sumLenSq > 1e-12f) {
        bentNormal = unoccludedDirSum * (1.f / std::sqrt(sumLenSq));
    } else {
        bentNormal = N;
    }

    return {ao, bentNormal};
}

} // namespace

AmbientOcclusionResult MeshAmbientOcclusion::bake(
    const Mesh& mesh,
    const AmbientOcclusionOptions& opts)
{
    AmbientOcclusionResult result;
    if (!mesh.isValid()) return result;

    const auto& pos = mesh.attributes().positions();
    const auto& norms = mesh.attributes().normals();

    if (!mesh.attributes().hasNormals()) return result;

    size_t V = pos.size();
    result.ao.resize(V, 1.f);
    result.bentNormals.resize(V);

    MeshBVH bvh;
    const MeshBVH* bvhPtr = nullptr;
    if (opts.useBVH) {
        bvh.build(mesh);
        if (!bvh.isValid()) return result;
        bvhPtr = &bvh;
    }

    if (opts.surfaceSampleCount > 0) {
        PointSampleOptions sampOpts;
        sampOpts.count = opts.surfaceSampleCount;
        sampOpts.seed = opts.seed;
        auto surfaceSamples = MeshPointSample::sample(mesh, sampOpts);
        if (surfaceSamples.points.empty()) return result;

        std::vector<float> sampleAO;
        std::vector<Vec3> sampleBN;
        sampleAO.reserve(surfaceSamples.points.size());
        sampleBN.reserve(surfaceSamples.points.size());

        for (size_t si = 0; si < surfaceSamples.points.size(); ++si) {
            internal::SplitMix64 rng(opts.seed ^ (static_cast<uint64_t>(si + 1) * 0x9e3779b97f4a7c15ULL));

            EvalResult eval = evaluateAtPoint(
                surfaceSamples.points[si],
                surfaceSamples.normals[si],
                opts.sampleCount,
                opts.maxDistance,
                rng,
                bvhPtr,
                mesh);

            sampleAO.push_back(eval.ao);
            sampleBN.push_back(eval.bentNormal);
        }

        for (size_t vi = 0; vi < V; ++vi) {
            float bestDistSq = std::numeric_limits<float>::max();
            size_t bestSample = 0;
            for (size_t si = 0; si < surfaceSamples.points.size(); ++si) {
                Vec3 d = pos[vi] - surfaceSamples.points[si];
                float distSq = d.lengthSq();
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestSample = si;
                }
            }
            result.ao[vi] = sampleAO[bestSample];
            result.bentNormals[vi] = sampleBN[bestSample];
        }
    } else {
        for (size_t vi = 0; vi < V; ++vi) {
            internal::SplitMix64 rng(opts.seed ^ (static_cast<uint64_t>(vi) * 0x9e3779b97f4a7c15ULL));

            EvalResult eval = evaluateAtPoint(
                pos[vi],
                norms[vi],
                opts.sampleCount,
                opts.maxDistance,
                rng,
                bvhPtr,
                mesh);

            result.ao[vi] = eval.ao;
            result.bentNormals[vi] = eval.bentNormal;
        }
    }

    return result;
}

} // namespace nexus::geometry
