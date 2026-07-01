#include <nexus/geometry/MeshDecimator.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>
#include <unordered_set>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

static constexpr float kQuadricDeterminantEpsilon = 1e-14f;
static constexpr float k5DGaussianEpsilon = 1e-12f;

static inline int idx5(int r, int c) {
    static const int kOff[5] = {0, 5, 9, 12, 14};
    return (r <= c) ? kOff[r] + (c - r) : kOff[c] + (r - c);
}

struct Quadric {
    float m[21] = {};

    void addPlane(const Vec3& normal, float d) {
        float a = normal.x, b = normal.y, c = normal.z;
        m[0] += a * a;
        m[1] += a * b;
        m[2] += a * c;
        m[3] += a * d;
        m[4] += b * b;
        m[5] += b * c;
        m[6] += b * d;
        m[7] += c * c;
        m[8] += c * d;
        m[9] += d * d;
    }

    [[nodiscard]] float evaluate(const Vec3& v) const {
        float x = v.x, y = v.y, z = v.z;
        return m[0]*x*x + 2.f*m[1]*x*y + 2.f*m[2]*x*z + 2.f*m[3]*x
             + m[4]*y*y + 2.f*m[5]*y*z + 2.f*m[6]*y
             + m[7]*z*z + 2.f*m[8]*z
             + m[9];
    }

    [[nodiscard]] bool solveOptimal(Vec3& result) const {
        float a11 = m[0], a12 = m[1], a13 = m[2], b1 = -m[3];
        float a21 = m[1], a22 = m[4], a23 = m[5], b2 = -m[6];
        float a31 = m[2], a32 = m[5], a33 = m[7], b3 = -m[8];

        float det = a11*(a22*a33 - a23*a32) - a12*(a21*a33 - a23*a31) + a13*(a21*a32 - a22*a31);
        if (std::abs(det) < kQuadricDeterminantEpsilon) return false;

        float invDet = 1.f / det;
        result.x = invDet * (b1*(a22*a33 - a23*a32) - a12*(b2*a33 - a23*b3) + a13*(b2*a32 - a22*b3));
        result.y = invDet * (a11*(b2*a33 - a23*b3) - b1*(a21*a33 - a23*a31) + a13*(a21*b3 - b2*a31));
        result.z = invDet * (a11*(a22*b3 - b2*a32) - a12*(a21*b3 - b2*a31) + b1*(a21*a32 - a22*a31));
        return true;
    }

    void addPlane5D(float nx, float ny, float nz, float nu, float nv, float d) {
        float n[5] = {nx, ny, nz, nu, nv};
        for (int r = 0; r < 5; ++r) {
            for (int c = r; c < 5; ++c) {
                m[idx5(r, c)] += n[r] * n[c];
            }
        }
        m[15] += n[0] * d;
        m[16] += n[1] * d;
        m[17] += n[2] * d;
        m[18] += n[3] * d;
        m[19] += n[4] * d;
        m[20] += d * d;
    }

    [[nodiscard]] float evaluate5D(float x, float y, float z, float u, float v) const {
        return m[idx5(0,0)]*x*x + 2.f*m[idx5(0,1)]*x*y + 2.f*m[idx5(0,2)]*x*z
             + 2.f*m[idx5(0,3)]*x*u + 2.f*m[idx5(0,4)]*x*v
             + m[idx5(1,1)]*y*y + 2.f*m[idx5(1,2)]*y*z + 2.f*m[idx5(1,3)]*y*u
             + 2.f*m[idx5(1,4)]*y*v
             + m[idx5(2,2)]*z*z + 2.f*m[idx5(2,3)]*z*u + 2.f*m[idx5(2,4)]*z*v
             + m[idx5(3,3)]*u*u + 2.f*m[idx5(3,4)]*u*v
             + m[idx5(4,4)]*v*v
             + 2.f*m[15]*x + 2.f*m[16]*y + 2.f*m[17]*z + 2.f*m[18]*u + 2.f*m[19]*v
             + m[20];
    }

    [[nodiscard]] bool solveOptimal5D(float result[5]) const {
        float A[5][5];
        float B[5];
        for (int r = 0; r < 5; ++r) {
            for (int c = 0; c < 5; ++c) {
                A[r][c] = m[idx5(r, c)];
            }
            B[r] = -m[15 + r];
        }

        for (int col = 0; col < 5; ++col) {
            int bestRow = col;
            float bestVal = std::abs(A[col][col]);
            for (int row = col + 1; row < 5; ++row) {
                float v = std::abs(A[row][col]);
                if (v > bestVal) { bestVal = v; bestRow = row; }
            }
            if (bestVal < k5DGaussianEpsilon) {
                for (int r = col; r < 5; ++r) {
                    bool allZero = true;
                    for (int c = 0; c < 5; ++c) {
                        if (std::abs(A[r][c]) >= k5DGaussianEpsilon) { allZero = false; break; }
                    }
                    if (!allZero) return false;
                }
                return false;
            }
            if (bestRow != col) {
                for (int c = 0; c < 5; ++c) std::swap(A[col][c], A[bestRow][c]);
                std::swap(B[col], B[bestRow]);
            }
            float pivot = A[col][col];
            for (int row = col + 1; row < 5; ++row) {
                float factor = A[row][col] / pivot;
                for (int c = col; c < 5; ++c) {
                    A[row][c] -= factor * A[col][c];
                }
                B[row] -= factor * B[col];
            }
        }

        for (int col = 4; col >= 0; --col) {
            float sum = B[col];
            for (int c = col + 1; c < 5; ++c) {
                sum -= A[col][c] * result[c];
            }
            result[col] = sum / A[col][col];
        }
        return true;
    }

    void combineWith(const Quadric& other) {
        for (int i = 0; i < 21; ++i) m[i] += other.m[i];
    }
};

struct EdgeEntry {
    uint32_t v0;
    uint32_t v1;
    Vec3     optimalPos;
    Vec2     optimalUV;
    float    cost;
    uint32_t version;

    [[nodiscard]] bool operator>(const EdgeEntry& o) const noexcept { return cost > o.cost; }
};

void computeFacePlane(const HalfEdgeMesh& hem, uint32_t fi, Vec3& normal, float& d) {
    uint32_t start = hem.face(fi).edge;
    if (start == HalfEdgeMesh::kInvalid) { normal = {0,1,0}; d = 0; return; }

    std::vector<uint32_t> verts;
    uint32_t e = start;
    do {
        verts.push_back(hem.edge(e).src);
        uint32_t nextEdge = hem.edge(e).next;
        if (nextEdge == HalfEdgeMesh::kInvalid || nextEdge >= hem.edgeCount()) break;
        e = nextEdge;
    } while (e != start && verts.size() < 256);

    if (verts.size() < 3) { normal = {0,1,0}; d = 0; return; }

    const Vec3& v0 = hem.positions()[verts[0]];
    const Vec3& v1 = hem.positions()[verts[1]];
    const Vec3& v2 = hem.positions()[verts[2]];
    Vec3 e1 = v1 - v0;
    Vec3 e2 = v2 - v0;
    normal = e1.cross(e2);
    float len = normal.length();
    if (len < kQuadricDeterminantEpsilon) { normal = {0,1,0}; d = 0; return; }
    normal = normal * (1.f / len);
    d = -normal.dot(v0);
}

float computeFaceArea(const HalfEdgeMesh& hem, uint32_t fi) {
    uint32_t start = hem.face(fi).edge;
    if (start == HalfEdgeMesh::kInvalid) return 0.f;

    std::vector<uint32_t> verts;
    uint32_t e = start;
    do {
        verts.push_back(hem.edge(e).src);
        uint32_t nextEdge = hem.edge(e).next;
        if (nextEdge == HalfEdgeMesh::kInvalid || nextEdge >= hem.edgeCount()) break;
        e = nextEdge;
    } while (e != start && verts.size() < 256);

    if (verts.size() < 3) return 0.f;

    const Vec3& v0 = hem.positions()[verts[0]];
    const Vec3& v1 = hem.positions()[verts[1]];
    const Vec3& v2 = hem.positions()[verts[2]];
    Vec3 e1 = v1 - v0;
    Vec3 e2 = v2 - v0;
    return e1.cross(e2).length() * 0.5f;
}

uint32_t countActiveFaces(const HalfEdgeMesh& hem) {
    uint32_t count = 0;
    for (uint32_t fi = 0; fi < hem.faceCount(); ++fi) {
        if (hem.face(fi).edge != HalfEdgeMesh::kInvalid) ++count;
    }
    return count;
}

void computeAllQuadrics(const HalfEdgeMesh& hem, std::vector<Quadric>& quadrics,
                        bool preserveAttributes, float uvWeight) {
    size_t V = quadrics.size();
    for (size_t vi = 0; vi < V; ++vi) quadrics[vi] = {};

    bool hasUVs = preserveAttributes && hem.hasUVs();

    for (uint32_t fi = 0; fi < hem.faceCount(); ++fi) {
        if (hem.face(fi).edge == HalfEdgeMesh::kInvalid) continue;

        Vec3 normal;
        float d;
        computeFacePlane(hem, fi, normal, d);

        uint32_t start = hem.face(fi).edge;
        std::vector<uint32_t> faceVerts;
        uint32_t e = start;
        do {
            faceVerts.push_back(hem.edge(e).src);
            e = hem.edge(e).next;
        } while (e != start);

        float area = 0.f;
        if (faceVerts.size() >= 3) {
            const auto& p0 = hem.positions()[faceVerts[0]];
            const auto& p1 = hem.positions()[faceVerts[1]];
            const auto& p2 = hem.positions()[faceVerts[2]];
            area = (p1 - p0).cross(p2 - p0).length() * 0.5f;
        }

        for (uint32_t v : faceVerts) {
            if (v >= quadrics.size()) continue;

            if (hasUVs) {
                quadrics[v].addPlane5D(normal.x, normal.y, normal.z, 0.f, 0.f, d);
                float w = area * uvWeight;
                const auto& uv = hem.uvs()[v];
                quadrics[v].addPlane5D(0.f, 0.f, 0.f, std::sqrt(w), 0.f, -std::sqrt(w) * uv.u);
                quadrics[v].addPlane5D(0.f, 0.f, 0.f, 0.f, std::sqrt(w), -std::sqrt(w) * uv.v);
            } else {
                quadrics[v].addPlane(normal, d);
            }
        }
    }
}

void recomputeQuadric(const HalfEdgeMesh& hem, std::vector<Quadric>& quadrics, uint32_t v,
                      bool preserveAttributes, float uvWeight) {
    if (v >= quadrics.size()) return;
    quadrics[v] = {};

    uint32_t start = hem.vertex(v).edge;
    if (start == HalfEdgeMesh::kInvalid) return;

    bool hasUVs = preserveAttributes && hem.hasUVs();

    uint32_t e = start;
    uint32_t safety = 0;
    do {
        uint32_t fi = hem.edge(e).face;
        if (fi != HalfEdgeMesh::kInvalid) {
            Vec3 normal;
            float d;
            computeFacePlane(hem, fi, normal, d);

            float area = computeFaceArea(hem, fi);

            if (hasUVs) {
                quadrics[v].addPlane5D(normal.x, normal.y, normal.z, 0.f, 0.f, d);
                float w = area * uvWeight;
                const auto& uv = hem.uvs()[v];
                quadrics[v].addPlane5D(0.f, 0.f, 0.f, std::sqrt(w), 0.f, -std::sqrt(w) * uv.u);
                quadrics[v].addPlane5D(0.f, 0.f, 0.f, 0.f, std::sqrt(w), -std::sqrt(w) * uv.v);
            } else {
                quadrics[v].addPlane(normal, d);
            }
        }
        uint32_t prevEdge = hem.edge(e).prev;
        if (prevEdge == HalfEdgeMesh::kInvalid) break;
        if (prevEdge >= hem.edgeCount()) break;
        uint32_t prevTwin = hem.edge(prevEdge).twin;
        if (prevTwin == HalfEdgeMesh::kInvalid) break;
        e = prevTwin;
        if (++safety > hem.edgeCount() * 2) break;
    } while (e != start);
}

} // namespace

std::optional<std::pair<HalfEdgeMesh, DecimationReport>>
MeshDecimator::decimate(const HalfEdgeMesh& mesh, const DecimationOptions& opts) {
    DecimationReport report;
    report.facesIn = countActiveFaces(mesh);
    if (report.facesIn == 0) return std::nullopt;

    size_t targetFaces = opts.targetFaceCount;
    if (targetFaces == 0) {
        targetFaces = static_cast<size_t>(static_cast<float>(report.facesIn) * opts.targetRatio);
    }
    if (targetFaces < 3) targetFaces = 3;
    if (targetFaces >= report.facesIn) {
        return std::make_pair(mesh, report);
    }

    HalfEdgeMesh workingMesh = mesh;
    if (!workingMesh.isTriangulated()) {
        Mesh m = workingMesh.toMesh();
        auto opt = HalfEdgeMesh::fromMesh(m);
        if (!opt) return std::nullopt;
        workingMesh = std::move(*opt);
    }

    bool use5D = opts.preserveAttributes && workingMesh.hasUVs();

    size_t V = workingMesh.vertexCount();
    std::vector<Quadric> quadrics(V);
    computeAllQuadrics(workingMesh, quadrics, use5D, opts.uvWeight);

    std::vector<uint32_t> vertexVersion(V, 0);
    uint32_t globalVersion = 0;

    auto computeEdgeCost = [&](uint32_t v0, uint32_t v1) -> EdgeEntry {
        Quadric Q;
        Q.combineWith(quadrics[v0]);
        Q.combineWith(quadrics[v1]);

        EdgeEntry entry{};
        entry.v0 = v0;
        entry.v1 = v1;
        entry.version = globalVersion;

        if (use5D) {
            float result[5];
            if (Q.solveOptimal5D(result)) {
                entry.optimalPos = {result[0], result[1], result[2]};
                entry.optimalUV = {result[3], result[4]};
                entry.cost = Q.evaluate5D(result[0], result[1], result[2], result[3], result[4]);
            } else {
                const Vec3& p0 = workingMesh.positions()[v0];
                const Vec3& p1 = workingMesh.positions()[v1];
                entry.optimalPos = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f, (p0.z + p1.z) * 0.5f};
                entry.optimalUV = {
                    (workingMesh.uvs()[v0].u + workingMesh.uvs()[v1].u) * 0.5f,
                    (workingMesh.uvs()[v0].v + workingMesh.uvs()[v1].v) * 0.5f
                };
                entry.cost = Q.evaluate5D(entry.optimalPos.x, entry.optimalPos.y, entry.optimalPos.z,
                                          entry.optimalUV.u, entry.optimalUV.v);

                float c0 = Q.evaluate5D(p0.x, p0.y, p0.z,
                                         workingMesh.uvs()[v0].u, workingMesh.uvs()[v0].v);
                float c1 = Q.evaluate5D(p1.x, p1.y, p1.z,
                                         workingMesh.uvs()[v1].u, workingMesh.uvs()[v1].v);
                if (c0 < entry.cost) {
                    entry.cost = c0;
                    entry.optimalPos = p0;
                    entry.optimalUV = workingMesh.uvs()[v0];
                }
                if (c1 < entry.cost) {
                    entry.cost = c1;
                    entry.optimalPos = p1;
                    entry.optimalUV = workingMesh.uvs()[v1];
                }
            }
        } else {
            Vec3 optimal;
            float cost;
            if (Q.solveOptimal(optimal)) {
                cost = Q.evaluate(optimal);
            } else {
                const Vec3& p0 = workingMesh.positions()[v0];
                const Vec3& p1 = workingMesh.positions()[v1];
                optimal = (p0 + p1) * 0.5f;
                cost = Q.evaluate(optimal);

                float c0 = Q.evaluate(p0);
                float c1 = Q.evaluate(p1);
                if (c0 < cost) { cost = c0; optimal = p0; }
                if (c1 < cost) { cost = c1; optimal = p1; }
            }
            entry.optimalPos = optimal;
            entry.cost = cost;
        }
        return entry;
    };

    struct MinHeapCmp {
        bool operator()(const EdgeEntry& a, const EdgeEntry& b) const { return a.cost > b.cost; }
    };
    std::priority_queue<EdgeEntry, std::vector<EdgeEntry>, MinHeapCmp> heap;

    auto pushAllEdges = [&]() {
        std::priority_queue<EdgeEntry, std::vector<EdgeEntry>, MinHeapCmp> fresh;
        heap.swap(fresh);

        for (uint32_t vi = 0; vi < static_cast<uint32_t>(V); ++vi) {
            if (workingMesh.vertex(vi).edge == HalfEdgeMesh::kInvalid) continue;
            auto neighbors = workingMesh.vertexNeighbors(vi);
            for (uint32_t nj : neighbors) {
                if (nj <= vi) continue;

                uint32_t he = workingMesh.findEdge(vi, nj);
                uint32_t heTwin = workingMesh.findEdge(nj, vi);
                if (he == HalfEdgeMesh::kInvalid || heTwin == HalfEdgeMesh::kInvalid) continue;

                if (opts.preserveBoundary) {
                    if (workingMesh.edge(he).twin == HalfEdgeMesh::kInvalid) continue;
                }

                EdgeEntry entry = computeEdgeCost(vi, nj);
                entry.version = globalVersion;
                heap.push(entry);
            }
        }
    };

    pushAllEdges();
    uint32_t activeFaces = report.facesIn;

    while (activeFaces > targetFaces && !heap.empty()) {
        EdgeEntry entry = heap.top();
        heap.pop();

        if (entry.version != std::max(vertexVersion[entry.v0], vertexVersion[entry.v1])) continue;

        if (entry.cost > opts.maxError) continue;

        uint32_t he = workingMesh.findEdge(entry.v0, entry.v1);
        if (he == HalfEdgeMesh::kInvalid) continue;

        uint32_t faceA = workingMesh.edge(he).face;
        uint32_t faceB = HalfEdgeMesh::kInvalid;
        uint32_t t = workingMesh.edge(he).twin;
        if (t != HalfEdgeMesh::kInvalid) faceB = workingMesh.edge(t).face;

        if (!workingMesh.collapseEdge(he, entry.optimalPos)) continue;

        ++report.collapses;
        if (entry.cost > report.maxErrorApplied) report.maxErrorApplied = entry.cost;

        if (use5D && workingMesh.hasUVs() && entry.v1 < workingMesh.uvs().size()) {
            workingMesh.uvs()[entry.v1] = entry.optimalUV;
        }

        uint32_t facesRemoved = 0;
        if (faceA != HalfEdgeMesh::kInvalid && workingMesh.face(faceA).edge == HalfEdgeMesh::kInvalid) ++facesRemoved;
        if (faceB != HalfEdgeMesh::kInvalid && workingMesh.face(faceB).edge == HalfEdgeMesh::kInvalid) ++facesRemoved;
        activeFaces = (activeFaces >= facesRemoved) ? (activeFaces - facesRemoved) : 0;

        ++globalVersion;
        uint32_t vKeep = entry.v1;

        std::unordered_set<uint32_t> dirtyVerts;
        dirtyVerts.insert(vKeep);
        auto neighbors = workingMesh.vertexNeighbors(vKeep);
        for (uint32_t nj : neighbors) {
            dirtyVerts.insert(nj);
        }
        if (entry.v0 < V) dirtyVerts.insert(entry.v0);

        for (uint32_t dv : dirtyVerts) {
            if (dv < V) vertexVersion[dv] = globalVersion;
        }

        for (uint32_t dv : dirtyVerts) {
            recomputeQuadric(workingMesh, quadrics, dv, use5D, opts.uvWeight);
        }

        for (uint32_t dv : dirtyVerts) {
            if (workingMesh.vertex(dv).edge == HalfEdgeMesh::kInvalid) continue;
            auto nbrs = workingMesh.vertexNeighbors(dv);
            for (uint32_t nj : nbrs) {
                if (nj <= dv) continue;

                uint32_t heNew = workingMesh.findEdge(dv, nj);
                uint32_t heTwinNew = workingMesh.findEdge(nj, dv);
                if (heNew == HalfEdgeMesh::kInvalid || heTwinNew == HalfEdgeMesh::kInvalid) continue;

                if (opts.preserveBoundary) {
                    if (workingMesh.edge(heNew).twin == HalfEdgeMesh::kInvalid) continue;
                }

                EdgeEntry newEntry = computeEdgeCost(dv, nj);
                newEntry.version = globalVersion;
                heap.push(newEntry);
            }
        }
    }

    report.facesOut = countActiveFaces(workingMesh);
    if (report.facesOut > targetFaces) return std::nullopt;
    return std::make_pair(std::move(workingMesh), report);
}

} // namespace nexus::geometry
