#include <nexus/geometry/SubdivisionSurface.h>
#include <nexus/geometry/Tolerance.h>  // geometry::isFinite (non-finite rejection convention)

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

Vec2 addVec2(const Vec2& a, const Vec2& b) { return {a.u + b.u, a.v + b.v}; }
Vec2 scaleVec2(const Vec2& a, float s) { return {a.u * s, a.v * s}; }

// Average of face vertices for Catmull-Clark face point
Vec3 faceCentroid(const HalfEdgeMesh& hem, uint32_t fi) {
    uint32_t start = hem.face(fi).edge;
    if (start == HalfEdgeMesh::kInvalid) return {0,0,0};

    Vec3 sum{0,0,0};
    uint32_t count = 0;
    uint32_t e = start;
    do {
        sum = sum + hem.positions()[hem.edge(e).src];
        ++count;
        e = hem.edge(e).next;
    } while (e != start && count < 256);
    if (count == 0) return {0,0,0};
    return sum * (1.f / static_cast<float>(count));
}

Vec2 faceCentroidUV(const HalfEdgeMesh& hem, uint32_t fi) {
    uint32_t start = hem.face(fi).edge;
    if (start == HalfEdgeMesh::kInvalid) return {0,0};
    Vec2 sum{0,0};
    uint32_t count = 0;
    uint32_t e = start;
    do {
        sum = addVec2(sum, hem.uvs()[hem.edge(e).src]);
        ++count;
        e = hem.edge(e).next;
    } while (e != start && count < 256);
    if (count == 0) return {0,0};
    return scaleVec2(sum, 1.f / static_cast<float>(count));
}

Vec3 faceCentroidNormal(const HalfEdgeMesh& hem, uint32_t fi) {
    uint32_t start = hem.face(fi).edge;
    if (start == HalfEdgeMesh::kInvalid) return {0,0,0};
    Vec3 sum{0,0,0};
    uint32_t count = 0;
    uint32_t e = start;
    do {
        sum = sum + hem.normals()[hem.edge(e).src];
        ++count;
        e = hem.edge(e).next;
    } while (e != start && count < 256);
    if (count == 0) return {0,0,0};
    return sum * (1.f / static_cast<float>(count));
}

} // namespace

// --- Catmull-Clark ------------------------------------------------------------

std::optional<HalfEdgeMesh>
SubdivisionSurface::catmullClark(const HalfEdgeMesh& mesh, const SubdivisionOptions& opts) {
    if (opts.levels == 0) return mesh;
    for (const auto& p : mesh.positions())
        if (!isFinite(p)) return std::nullopt;  // reject non-finite input (convention)

    HalfEdgeMesh current = mesh;
    CreaseEdgeSet currentCreases = opts.creases;

    for (uint32_t level = 0; level < opts.levels; ++level) {
        uint32_t V = current.vertexCount();
        uint32_t F = current.faceCount();
        uint32_t E = current.edgeCount();

        const bool hasUVs = current.hasUVs();
        const bool hasNormals = current.hasNormals();

        // --- Face points: centroid of each face
        std::vector<Vec3> facePoints(F);
        std::vector<Vec2> faceUVs;
        std::vector<Vec3> faceNormals;
        if (hasUVs) faceUVs.resize(F);
        if (hasNormals) faceNormals.resize(F);
        for (uint32_t fi = 0; fi < F; ++fi) {
            if (current.face(fi).edge != HalfEdgeMesh::kInvalid) {
                facePoints[fi] = faceCentroid(current, fi);
                if (hasUVs) faceUVs[fi] = faceCentroidUV(current, fi);
                if (hasNormals) faceNormals[fi] = faceCentroidNormal(current, fi);
            }
        }

        // --- Edge points and face adjacency per edge
        std::vector<Vec3> edgePoints(E);
        std::vector<uint32_t> edgeFaceA(E, HalfEdgeMesh::kInvalid);
        std::vector<uint32_t> edgeFaceB(E, HalfEdgeMesh::kInvalid);

        std::unordered_map<uint64_t, uint32_t> edgeIndexMap;
        for (uint32_t he = 0; he < E; ++he) {
            const auto& e = current.edge(he);
            if (e.src == HalfEdgeMesh::kInvalid || e.face == HalfEdgeMesh::kInvalid) continue;

            uint32_t a = e.src;
            uint32_t b = current.edge(e.next).src;
            uint64_t key = (static_cast<uint64_t>(std::min(a,b)) << 32) | static_cast<uint64_t>(std::max(a,b));
            auto it = edgeIndexMap.find(key);
            if (it == edgeIndexMap.end()) {
                edgeIndexMap[key] = he;
                edgeFaceA[he] = e.face;
            } else {
                uint32_t otherHe = it->second;
                edgeFaceA[otherHe] = current.edge(otherHe).face;
                edgeFaceB[otherHe] = e.face;
                edgeFaceA[he] = e.face;
                edgeFaceB[he] = current.edge(otherHe).face;
            }
        }

        // Compute edge points
        std::vector<Vec2> edgeUVs;
        std::vector<Vec3> edgeNormals;
        if (hasUVs) edgeUVs.resize(E);
        if (hasNormals) edgeNormals.resize(E);

        for (const auto& [key, he] : edgeIndexMap) {
            uint32_t a = static_cast<uint32_t>(key >> 32);
            uint32_t b = static_cast<uint32_t>(key & 0xFFFFFFFFu);
            Vec3 avgEndpoints = (current.positions()[a] + current.positions()[b]) * 0.5f;

            uint32_t fa = edgeFaceA[he];
            uint32_t fb = edgeFaceB[he];

            float cs = currentCreases.get(a, b);

            if (cs > 0.0f || fa == HalfEdgeMesh::kInvalid || fb == HalfEdgeMesh::kInvalid) {
                edgePoints[he] = avgEndpoints;
                if (hasUVs) edgeUVs[he] = scaleVec2(addVec2(current.uvs()[a], current.uvs()[b]), 0.5f);
                if (hasNormals) edgeNormals[he] = (current.normals()[a] + current.normals()[b]) * 0.5f;
            } else {
                Vec3 avgFaces = (facePoints[fa] + facePoints[fb]) * 0.5f;
                edgePoints[he] = (avgEndpoints + avgFaces) * 0.5f;

                if (hasUVs) {
                    Vec2 avgEndUV = scaleVec2(addVec2(current.uvs()[a], current.uvs()[b]), 0.5f);
                    Vec2 avgFaceUV = scaleVec2(addVec2(faceUVs[fa], faceUVs[fb]), 0.5f);
                    edgeUVs[he] = scaleVec2(addVec2(avgEndUV, avgFaceUV), 0.5f);
                }
                if (hasNormals) {
                    Vec3 avgEndN = (current.normals()[a] + current.normals()[b]) * 0.5f;
                    Vec3 avgFaceN = (faceNormals[fa] + faceNormals[fb]) * 0.5f;
                    edgeNormals[he] = (avgEndN + avgFaceN) * 0.5f;
                }
            }
        }

        // --- New vertex positions and attributes
        std::vector<Vec3> newVertexPos(V);
        std::vector<Vec2> newVertexUV;
        std::vector<Vec3> newVertexNormal;
        if (hasUVs) newVertexUV.resize(V);
        if (hasNormals) newVertexNormal.resize(V);

        for (uint32_t vi = 0; vi < V; ++vi) {
            if (current.vertex(vi).edge == HalfEdgeMesh::kInvalid) {
                newVertexPos[vi] = current.positions()[vi];
                if (hasUVs) newVertexUV[vi] = current.uvs()[vi];
                if (hasNormals) newVertexNormal[vi] = current.normals()[vi];
                continue;
            }

            auto neighbors = current.vertexNeighbors(vi);
            uint32_t n = static_cast<uint32_t>(neighbors.size());
            if (n == 0) {
                newVertexPos[vi] = current.positions()[vi];
                if (hasUVs) newVertexUV[vi] = current.uvs()[vi];
                if (hasNormals) newVertexNormal[vi] = current.normals()[vi];
                continue;
            }

            uint32_t startHe = current.vertex(vi).edge;
            uint32_t he = startHe;

            bool boundary = false;
            uint32_t bn0 = HalfEdgeMesh::kInvalid, bn1 = HalfEdgeMesh::kInvalid;
            uint32_t cn0 = HalfEdgeMesh::kInvalid, cn1 = HalfEdgeMesh::kInvalid;
            float maxCreaseSharpness = 0.0f;

            do {
                const auto& e = current.edge(he);
                uint32_t nb = current.edge(e.next).src;

                if (e.twin == HalfEdgeMesh::kInvalid) {
                    boundary = true;
                    if (bn0 == HalfEdgeMesh::kInvalid) bn0 = nb;
                    else if (bn1 == HalfEdgeMesh::kInvalid) bn1 = nb;
                }

                if (!currentCreases.empty()) {
                    float s = currentCreases.get(vi, nb);
                    if (s > 0.0f) {
                        if (s > maxCreaseSharpness) maxCreaseSharpness = s;
                        if (cn0 == HalfEdgeMesh::kInvalid) cn0 = nb;
                        else if (cn1 == HalfEdgeMesh::kInvalid && nb != cn0) cn1 = nb;
                    }
                }

                uint32_t prevTwin = current.edge(current.edge(he).prev).twin;
                if (prevTwin == HalfEdgeMesh::kInvalid) { boundary = true; break; }
                he = prevTwin;
            } while (he != startHe);

            bool creaseChain = !boundary && cn0 != HalfEdgeMesh::kInvalid && cn1 != HalfEdgeMesh::kInvalid;

            if (boundary) {
                if (bn0 != HalfEdgeMesh::kInvalid && bn1 != HalfEdgeMesh::kInvalid) {
                    Vec3 oldPos = current.positions()[vi];
                    newVertexPos[vi] = oldPos * (3.f / 4.f)
                                     + (current.positions()[bn0] + current.positions()[bn1]) * (1.f / 8.f);
                    if (hasUVs) {
                        const Vec2& oldUV = current.uvs()[vi];
                        newVertexUV[vi] = scaleVec2(addVec2(
                            scaleVec2(oldUV, 3.f/4.f),
                            scaleVec2(addVec2(current.uvs()[bn0], current.uvs()[bn1]), 1.f/8.f)), 1.f);
                    }
                    if (hasNormals) {
                        const Vec3& oldN = current.normals()[vi];
                        newVertexNormal[vi] = oldN * (3.f/4.f)
                                            + (current.normals()[bn0] + current.normals()[bn1]) * (1.f/8.f);
                    }
                } else {
                    newVertexPos[vi] = current.positions()[vi];
                    if (hasUVs) newVertexUV[vi] = current.uvs()[vi];
                    if (hasNormals) newVertexNormal[vi] = current.normals()[vi];
                }
            } else {
                Vec3 Qsum{0,0,0};
                Vec3 Rsum{0,0,0};
                Vec2 QuvSum{0,0};
                Vec2 RuvSum{0,0};
                Vec3 QnSum{0,0,0};
                Vec3 RnSum{0,0,0};

                he = startHe;
                do {
                    const auto& e = current.edge(he);
                    uint32_t nb = current.edge(e.next).src;
                    Qsum = Qsum + (current.positions()[vi] + current.positions()[nb]) * 0.5f;

                    if (hasUVs) {
                        QuvSum = addVec2(QuvSum,
                            scaleVec2(addVec2(current.uvs()[vi], current.uvs()[nb]), 0.5f));
                    }
                    if (hasNormals) {
                        QnSum = QnSum + (current.normals()[vi] + current.normals()[nb]) * 0.5f;
                    }

                    if (e.face != HalfEdgeMesh::kInvalid) {
                        Rsum = Rsum + facePoints[e.face];
                        if (hasUVs) RuvSum = addVec2(RuvSum, faceUVs[e.face]);
                        if (hasNormals) RnSum = RnSum + faceNormals[e.face];
                    }

                    uint32_t prevTwin = current.edge(current.edge(he).prev).twin;
                    if (prevTwin == HalfEdgeMesh::kInvalid) break;
                    he = prevTwin;
                } while (he != startHe);

                float nf = static_cast<float>(n);
                Vec3 oldPos = current.positions()[vi];
                if (nf < 1.f) { newVertexPos[vi] = oldPos; continue; }

                if (creaseChain) {
                    Vec3 smoothPos = oldPos * ((nf - 3.f) / nf)
                                    + Qsum * (2.f / (nf * nf))
                                    + Rsum * (1.f / (nf * nf));
                    Vec3 creasePos = oldPos * (3.f / 4.f)
                                    + (current.positions()[cn0] + current.positions()[cn1]) * (1.f / 8.f);

                    if (maxCreaseSharpness >= 0.5f) {
                        newVertexPos[vi] = creasePos;
                    } else {
                        float t = maxCreaseSharpness / 0.5f;
                        newVertexPos[vi] = smoothPos * (1.0f - t) + creasePos * t;
                    }

                    if (hasUVs) {
                        const Vec2& oldUV = current.uvs()[vi];
                        Vec2 smoothUV = addVec2(
                            addVec2(
                                scaleVec2(oldUV, (nf - 3.f) / nf),
                                scaleVec2(QuvSum, 2.f / (nf * nf))),
                            scaleVec2(RuvSum, 1.f / (nf * nf)));
                        Vec2 creaseUV = addVec2(
                            scaleVec2(oldUV, 3.f/4.f),
                            scaleVec2(addVec2(current.uvs()[cn0], current.uvs()[cn1]), 1.f/8.f));
                        if (maxCreaseSharpness >= 0.5f) {
                            newVertexUV[vi] = creaseUV;
                        } else {
                            float t = maxCreaseSharpness / 0.5f;
                            newVertexUV[vi] = addVec2(
                                scaleVec2(smoothUV, 1.0f - t),
                                scaleVec2(creaseUV, t));
                        }
                    }
                    if (hasNormals) {
                        const Vec3& oldN = current.normals()[vi];
                        Vec3 smoothN = oldN * ((nf - 3.f) / nf)
                                      + QnSum * (2.f / (nf * nf))
                                      + RnSum * (1.f / (nf * nf));
                        Vec3 creaseN = oldN * (3.f/4.f)
                                      + (current.normals()[cn0] + current.normals()[cn1]) * (1.f/8.f);
                        if (maxCreaseSharpness >= 0.5f) {
                            newVertexNormal[vi] = creaseN;
                        } else {
                            float t = maxCreaseSharpness / 0.5f;
                            newVertexNormal[vi] = smoothN * (1.0f - t) + creaseN * t;
                        }
                    }
                } else {
                    newVertexPos[vi] = oldPos * ((nf - 3.f) / nf)
                                     + Qsum * (2.f / (nf * nf))
                                     + Rsum * (1.f / (nf * nf));

                    if (hasUVs) {
                        const Vec2& oldUV = current.uvs()[vi];
                        newVertexUV[vi] = addVec2(
                            addVec2(
                                scaleVec2(oldUV, (nf - 3.f) / nf),
                                scaleVec2(QuvSum, 2.f / (nf * nf))),
                            scaleVec2(RuvSum, 1.f / (nf * nf)));
                    }
                    if (hasNormals) {
                        const Vec3& oldN = current.normals()[vi];
                        newVertexNormal[vi] = oldN * ((nf - 3.f) / nf)
                                            + QnSum * (2.f / (nf * nf))
                                            + RnSum * (1.f / (nf * nf));
                    }
                }
            }
        }

        // --- Build new mesh topology
        Mesh newMesh;
        std::vector<Vec3> allVerts;
        std::vector<Vec2> allUVs;
        std::vector<Vec3> allNormals;
        std::vector<std::vector<uint32_t>> quads;

        // Map old vertex index -> new index
        std::vector<uint32_t> vertMap(V, HalfEdgeMesh::kInvalid);

        auto addVert = [&](const Vec3& pos, const Vec2& uv, const Vec3& nrm) -> uint32_t {
            uint32_t idx = static_cast<uint32_t>(allVerts.size());
            allVerts.push_back(pos);
            allUVs.push_back(uv);
            allNormals.push_back(nrm);
            return idx;
        };

        // Add new vertex positions and attributes
        for (uint32_t vi = 0; vi < V; ++vi) {
            if (current.vertex(vi).edge != HalfEdgeMesh::kInvalid) {
                vertMap[vi] = addVert(newVertexPos[vi],
                    hasUVs ? newVertexUV[vi] : Vec2{0,0},
                    hasNormals ? newVertexNormal[vi] : Vec3{0,0,0});
            }
        }

        // Map for edge points (face-edge pair -> new vertex index)
        std::map<uint64_t, uint32_t> edgeVertMap;

        // Map for face points
        std::vector<uint32_t> faceVertMap(F, HalfEdgeMesh::kInvalid);
        for (uint32_t fi = 0; fi < F; ++fi) {
            if (current.face(fi).edge != HalfEdgeMesh::kInvalid) {
                faceVertMap[fi] = addVert(facePoints[fi],
                    hasUVs ? faceUVs[fi] : Vec2{0,0},
                    hasNormals ? faceNormals[fi] : Vec3{0,0,0});
            }
        }

        // For each face, create quads
        for (uint32_t fi = 0; fi < F; ++fi) {
            uint32_t fStart = current.face(fi).edge;
            if (fStart == HalfEdgeMesh::kInvalid) continue;

            // Collect face vertices in order
            std::vector<uint32_t> faceVerts;
            uint32_t e = fStart;
            do {
                faceVerts.push_back(e); // store half-edge index
                e = current.edge(e).next;
            } while (e != fStart && faceVerts.size() < 256);
            uint32_t n = static_cast<uint32_t>(faceVerts.size());
            if (n < 3) continue;

            // Get or create edge point vertices
            std::vector<uint32_t> edgeVertIdx(n);
            for (uint32_t j = 0; j < n; ++j) {
                uint32_t he = faceVerts[j];
                uint32_t a = current.edge(he).src;
                uint32_t b = current.edge(current.edge(he).next).src;
                uint64_t edgeKey = (static_cast<uint64_t>(std::min(a,b)) << 32) | static_cast<uint64_t>(std::max(a,b));

                auto it = edgeVertMap.find(edgeKey);
                if (it != edgeVertMap.end()) {
                    edgeVertIdx[j] = it->second;
                } else {
                    uint32_t mappedHe = edgeIndexMap.at(edgeKey);
                    uint32_t newIdx = addVert(edgePoints[mappedHe],
                        hasUVs ? edgeUVs[mappedHe] : Vec2{0,0},
                        hasNormals ? edgeNormals[mappedHe] : Vec3{0,0,0});
                    edgeVertMap[edgeKey] = newIdx;
                    edgeVertIdx[j] = newIdx;
                }
            }

            // Create quads
            uint32_t fvIdx = faceVertMap[fi];
            for (uint32_t j = 0; j < n; ++j) {
                uint32_t v = current.edge(faceVerts[j]).src;
                uint32_t vIdx = vertMap[v];
                uint32_t ej = edgeVertIdx[j];
                uint32_t ejPrev = edgeVertIdx[(j + n - 1) % n];
                quads.push_back({vIdx, ej, fvIdx, ejPrev});
            }
        }

        // Add all vertices and quads to the new mesh
        newMesh.attributes().setPositions(allVerts);
        if (hasUVs) newMesh.attributes().setUVs(allUVs);
        if (hasNormals) newMesh.attributes().setNormals(allNormals);
        for (const auto& quad : quads) {
            Face f;
            f.indices = quad;
            newMesh.topology().addFace(std::move(f));
        }

        // Propagate crease edges to the next level
        CreaseEdgeSet nextCreases;
        for (const auto& [key, s] : currentCreases.edges) {
            float sDecayed = std::max(0.0f, s - 1.0f);
            if (sDecayed > 0.0f) {
                uint32_t a = static_cast<uint32_t>(key >> 32);
                uint32_t b = static_cast<uint32_t>(key & 0xFFFFFFFFu);
                if (vertMap[a] != HalfEdgeMesh::kInvalid && vertMap[b] != HalfEdgeMesh::kInvalid) {
                    auto it = edgeVertMap.find(key);
                    if (it != edgeVertMap.end()) {
                        uint32_t mid = it->second;
                        nextCreases.set(vertMap[a], mid, sDecayed);
                        nextCreases.set(mid, vertMap[b], sDecayed);
                    }
                }
            }
        }

        auto nextOpt = HalfEdgeMesh::fromMesh(newMesh);
        if (!nextOpt) return std::nullopt;
        current = std::move(*nextOpt);
        currentCreases = std::move(nextCreases);
    }

    return current;
}

// --- Loop subdivision --------------------------------------------------------

std::optional<HalfEdgeMesh>
SubdivisionSurface::loopSubdivide(const HalfEdgeMesh& mesh, const SubdivisionOptions& opts) {
    if (opts.levels == 0) return mesh;
    if (!mesh.isTriangulated()) return std::nullopt;
    for (const auto& p : mesh.positions())
        if (!isFinite(p)) return std::nullopt;  // reject non-finite input (convention)

    HalfEdgeMesh current = mesh;
    CreaseEdgeSet currentCreases = opts.creases;

    for (uint32_t level = 0; level < opts.levels; ++level) {
        uint32_t V = current.vertexCount();
        uint32_t E = current.edgeCount();

        const bool hasUVs = current.hasUVs();
        const bool hasNormals = current.hasNormals();

        // --- Build edge map and compute edge points
        struct EdgeInfo {
            uint32_t v0, v1, vl, vr;
            bool boundary;
            float creaseSharpness = 0.0f;
        };

        std::map<uint64_t, EdgeInfo> edgeInfos;

        for (uint32_t he = 0; he < E; ++he) {
            const auto& e = current.edge(he);
            if (e.src == HalfEdgeMesh::kInvalid) continue;

            uint32_t v0 = e.src;
            uint32_t v1 = current.edge(e.next).src;
            uint64_t key = (static_cast<uint64_t>(std::min(v0,v1)) << 32) | static_cast<uint64_t>(std::max(v0,v1));
            if (edgeInfos.count(key)) continue; // process each edge once

            bool boundary = (e.twin == HalfEdgeMesh::kInvalid);

            uint32_t vl = HalfEdgeMesh::kInvalid;
            uint32_t vr = HalfEdgeMesh::kInvalid;

            if (!boundary) {
                vl = current.edge(current.edge(e.next).next).src;
                uint32_t twin = e.twin;
                if (twin != HalfEdgeMesh::kInvalid && current.edge(twin).next != HalfEdgeMesh::kInvalid) {
                    vr = current.edge(current.edge(current.edge(twin).next).next).src;
                }
            }

            float cs = currentCreases.get(v0, v1);
            edgeInfos[key] = {v0, v1, vl, vr, boundary, cs};
        }

        std::map<uint64_t, Vec3> edgePoints;
        std::map<uint64_t, Vec2> edgeUVs;
        std::map<uint64_t, Vec3> edgeNormals;
        for (const auto& [key, info] : edgeInfos) {
            const Vec3& p0 = current.positions()[info.v0];
            const Vec3& p1 = current.positions()[info.v1];

            if (info.boundary || info.vr == HalfEdgeMesh::kInvalid || info.creaseSharpness > 0.0f) {
                edgePoints[key] = (p0 + p1) * 0.5f;

                if (hasUVs) {
                    edgeUVs[key] = scaleVec2(addVec2(current.uvs()[info.v0], current.uvs()[info.v1]), 0.5f);
                }
                if (hasNormals) {
                    edgeNormals[key] = (current.normals()[info.v0] + current.normals()[info.v1]) * 0.5f;
                }
            } else {
                const Vec3& pl = current.positions()[info.vl];
                const Vec3& pr = current.positions()[info.vr];
                edgePoints[key] = (p0 + p1) * (3.f/8.f) + (pl + pr) * (1.f/8.f);

                if (hasUVs) {
                    edgeUVs[key] = addVec2(
                        scaleVec2(addVec2(current.uvs()[info.v0], current.uvs()[info.v1]), 3.f/8.f),
                        scaleVec2(addVec2(current.uvs()[info.vl], current.uvs()[info.vr]), 1.f/8.f));
                }
                if (hasNormals) {
                    edgeNormals[key] = (current.normals()[info.v0] + current.normals()[info.v1]) * (3.f/8.f)
                                     + (current.normals()[info.vl] + current.normals()[info.vr]) * (1.f/8.f);
                }
            }
        }

        // --- New vertex positions and attributes
        std::vector<Vec3> newVertPos(V);
        std::vector<Vec2> newVertUV;
        std::vector<Vec3> newVertNormal;
        if (hasUVs) newVertUV.resize(V);
        if (hasNormals) newVertNormal.resize(V);

        for (uint32_t vi = 0; vi < V; ++vi) {
            if (current.vertex(vi).edge == HalfEdgeMesh::kInvalid) {
                newVertPos[vi] = current.positions()[vi];
                if (hasUVs) newVertUV[vi] = current.uvs()[vi];
                if (hasNormals) newVertNormal[vi] = current.normals()[vi];
                continue;
            }

            auto neighbors = current.vertexNeighbors(vi);
            uint32_t n = static_cast<uint32_t>(neighbors.size());
            if (n == 0) {
                newVertPos[vi] = current.positions()[vi];
                if (hasUVs) newVertUV[vi] = current.uvs()[vi];
                if (hasNormals) newVertNormal[vi] = current.normals()[vi];
                continue;
            }

            uint32_t startHe = current.vertex(vi).edge;
            uint32_t he = startHe;

            bool boundary = false;
            uint32_t bn0 = HalfEdgeMesh::kInvalid, bn1 = HalfEdgeMesh::kInvalid;
            uint32_t cn0 = HalfEdgeMesh::kInvalid, cn1 = HalfEdgeMesh::kInvalid;
            float maxCreaseSharpness = 0.0f;

            do {
                const auto& e = current.edge(he);
                uint32_t nb = current.edge(e.next).src;

                if (e.twin == HalfEdgeMesh::kInvalid) {
                    boundary = true;
                    if (bn0 == HalfEdgeMesh::kInvalid) bn0 = nb;
                    else if (bn1 == HalfEdgeMesh::kInvalid) bn1 = nb;
                }

                if (!currentCreases.empty()) {
                    float s = currentCreases.get(vi, nb);
                    if (s > 0.0f) {
                        if (s > maxCreaseSharpness) maxCreaseSharpness = s;
                        if (cn0 == HalfEdgeMesh::kInvalid) cn0 = nb;
                        else if (cn1 == HalfEdgeMesh::kInvalid && nb != cn0) cn1 = nb;
                    }
                }

                uint32_t prevTwin = current.edge(current.edge(he).prev).twin;
                if (prevTwin == HalfEdgeMesh::kInvalid) { boundary = true; break; }
                he = prevTwin;
            } while (he != startHe);

            bool creaseChain = !boundary && cn0 != HalfEdgeMesh::kInvalid && cn1 != HalfEdgeMesh::kInvalid;

            if (boundary) {
                if (bn0 != HalfEdgeMesh::kInvalid && bn1 != HalfEdgeMesh::kInvalid) {
                    newVertPos[vi] = current.positions()[vi] * (3.f/4.f)
                                    + (current.positions()[bn0] + current.positions()[bn1]) * (1.f/8.f);
                    if (hasUVs) {
                        newVertUV[vi] = addVec2(
                            scaleVec2(current.uvs()[vi], 3.f/4.f),
                            scaleVec2(addVec2(current.uvs()[bn0], current.uvs()[bn1]), 1.f/8.f));
                    }
                    if (hasNormals) {
                        newVertNormal[vi] = current.normals()[vi] * (3.f/4.f)
                                          + (current.normals()[bn0] + current.normals()[bn1]) * (1.f/8.f);
                    }
                } else {
                    newVertPos[vi] = current.positions()[vi];
                    if (hasUVs) newVertUV[vi] = current.uvs()[vi];
                    if (hasNormals) newVertNormal[vi] = current.normals()[vi];
                }
            } else {
                float beta;
                if (n == 3) {
                    beta = 3.f/16.f;
                } else {
                    float t = 3.f/8.f + std::cos(2.f * 3.14159265359f / static_cast<float>(n)) / 4.f;
                    beta = (5.f/8.f - t*t) / static_cast<float>(n);
                }

                Vec3 neighborSum{0,0,0};
                Vec2 neighborUVSum{0,0};
                Vec3 neighborNrmSum{0,0,0};

                for (uint32_t nb : neighbors) {
                    neighborSum = neighborSum + current.positions()[nb];
                    if (hasUVs) neighborUVSum = addVec2(neighborUVSum, current.uvs()[nb]);
                    if (hasNormals) neighborNrmSum = neighborNrmSum + current.normals()[nb];
                }

                if (creaseChain) {
                    Vec3 smoothPos = current.positions()[vi] * (1.f - static_cast<float>(n) * beta)
                                    + neighborSum * beta;
                    Vec3 creasePos = current.positions()[vi] * (3.f/4.f)
                                    + (current.positions()[cn0] + current.positions()[cn1]) * (1.f/8.f);

                    if (maxCreaseSharpness >= 0.5f) {
                        newVertPos[vi] = creasePos;
                    } else {
                        float blend = maxCreaseSharpness / 0.5f;
                        newVertPos[vi] = smoothPos * (1.0f - blend) + creasePos * blend;
                    }

                    if (hasUVs) {
                        Vec2 smoothUV = addVec2(
                            scaleVec2(current.uvs()[vi], 1.f - static_cast<float>(n) * beta),
                            scaleVec2(neighborUVSum, beta));
                        Vec2 creaseUV = addVec2(
                            scaleVec2(current.uvs()[vi], 3.f/4.f),
                            scaleVec2(addVec2(current.uvs()[cn0], current.uvs()[cn1]), 1.f/8.f));
                        if (maxCreaseSharpness >= 0.5f) {
                            newVertUV[vi] = creaseUV;
                        } else {
                            float blend = maxCreaseSharpness / 0.5f;
                            newVertUV[vi] = addVec2(
                                scaleVec2(smoothUV, 1.0f - blend),
                                scaleVec2(creaseUV, blend));
                        }
                    }
                    if (hasNormals) {
                        Vec3 smoothN = current.normals()[vi] * (1.f - static_cast<float>(n) * beta)
                                      + neighborNrmSum * beta;
                        Vec3 creaseN = current.normals()[vi] * (3.f/4.f)
                                      + (current.normals()[cn0] + current.normals()[cn1]) * (1.f/8.f);
                        if (maxCreaseSharpness >= 0.5f) {
                            newVertNormal[vi] = creaseN;
                        } else {
                            float blend = maxCreaseSharpness / 0.5f;
                            newVertNormal[vi] = smoothN * (1.0f - blend) + creaseN * blend;
                        }
                    }
                } else {
                    newVertPos[vi] = current.positions()[vi] * (1.f - static_cast<float>(n) * beta)
                                    + neighborSum * beta;

                    if (hasUVs) {
                        newVertUV[vi] = addVec2(
                            scaleVec2(current.uvs()[vi], 1.f - static_cast<float>(n) * beta),
                            scaleVec2(neighborUVSum, beta));
                    }
                    if (hasNormals) {
                        newVertNormal[vi] = current.normals()[vi] * (1.f - static_cast<float>(n) * beta)
                                          + neighborNrmSum * beta;
                    }
                }
            }
        }

        // --- Build new mesh with 4x triangles per original triangle
        Mesh newMesh;
        std::vector<Vec3> allVerts;
        std::vector<Vec2> allUVs;
        std::vector<Vec3> allNormals;

        // Map: old vertex index -> new vertex index
        std::vector<uint32_t> vertMap(V, HalfEdgeMesh::kInvalid);

        auto addVert = [&](const Vec3& pos, const Vec2& uv, const Vec3& nrm) -> uint32_t {
            uint32_t idx = static_cast<uint32_t>(allVerts.size());
            allVerts.push_back(pos);
            allUVs.push_back(uv);
            allNormals.push_back(nrm);
            return idx;
        };

        for (uint32_t vi = 0; vi < V; ++vi) {
            if (current.vertex(vi).edge != HalfEdgeMesh::kInvalid) {
                vertMap[vi] = addVert(newVertPos[vi],
                    hasUVs ? newVertUV[vi] : Vec2{0,0},
                    hasNormals ? newVertNormal[vi] : Vec3{0,0,0});
            }
        }

        // Map: edge key -> new vertex index for edge points
        std::map<uint64_t, uint32_t> edgeVertMap;
        for (const auto& [key, ep] : edgePoints) {
            edgeVertMap[key] = addVert(ep,
                hasUVs ? edgeUVs[key] : Vec2{0,0},
                hasNormals ? edgeNormals[key] : Vec3{0,0,0});
        }

        // Process each face (triangle) and create 4 sub-triangles
        for (uint32_t fi = 0; fi < current.faceCount(); ++fi) {
            uint32_t fStart = current.face(fi).edge;
            if (fStart == HalfEdgeMesh::kInvalid) continue;

            // Get the three vertices and edges
            uint32_t v0 = current.edge(fStart).src;
            uint32_t he1 = current.edge(fStart).next;
            uint32_t he2 = current.edge(he1).next;

            uint32_t v1 = current.edge(he1).src;
            uint32_t v2 = current.edge(he2).src;

            auto edgeKey = [](uint32_t a, uint32_t b) {
                return (static_cast<uint64_t>(std::min(a,b)) << 32) | static_cast<uint64_t>(std::max(a,b));
            };

            uint32_t ev0 = vertMap[v0];
            uint32_t ev1 = vertMap[v1];
            uint32_t ev2 = vertMap[v2];

            if (ev0 == HalfEdgeMesh::kInvalid || ev1 == HalfEdgeMesh::kInvalid || ev2 == HalfEdgeMesh::kInvalid) continue;

            auto getEdgeVert = [&](uint32_t a, uint32_t b) -> uint32_t {
                uint64_t k = edgeKey(a, b);
                auto it = edgeVertMap.find(k);
                if (it != edgeVertMap.end()) return it->second;
                return HalfEdgeMesh::kInvalid;
            };

            uint32_t e01 = getEdgeVert(v0, v1);
            uint32_t e12 = getEdgeVert(v1, v2);
            uint32_t e20 = getEdgeVert(v2, v0);

            if (e01 == HalfEdgeMesh::kInvalid || e12 == HalfEdgeMesh::kInvalid || e20 == HalfEdgeMesh::kInvalid) continue;

            // 4 sub-triangles
            std::vector<std::vector<uint32_t>> tris = {
                {ev0, e01, e20},
                {ev1, e12, e01},
                {ev2, e20, e12},
                {e01, e12, e20},
            };

            for (auto& tri : tris) {
                Face f;
                f.indices = tri;
                newMesh.topology().addFace(std::move(f));
            }
        }

        newMesh.attributes().setPositions(allVerts);
        if (hasUVs) newMesh.attributes().setUVs(allUVs);
        if (hasNormals) newMesh.attributes().setNormals(allNormals);

        // Propagate crease edges to the next level
        CreaseEdgeSet nextCreases;
        for (const auto& [key, s] : currentCreases.edges) {
            float sDecayed = std::max(0.0f, s - 1.0f);
            if (sDecayed > 0.0f) {
                uint32_t a = static_cast<uint32_t>(key >> 32);
                uint32_t b = static_cast<uint32_t>(key & 0xFFFFFFFFu);
                if (vertMap[a] != HalfEdgeMesh::kInvalid && vertMap[b] != HalfEdgeMesh::kInvalid) {
                    auto it = edgeVertMap.find(key);
                    if (it != edgeVertMap.end()) {
                        uint32_t mid = it->second;
                        nextCreases.set(vertMap[a], mid, sDecayed);
                        nextCreases.set(mid, vertMap[b], sDecayed);
                    }
                }
            }
        }

        auto nextOpt = HalfEdgeMesh::fromMesh(newMesh);
        if (!nextOpt) return std::nullopt;
        current = std::move(*nextOpt);
        currentCreases = std::move(nextCreases);
    }

    return current;
}

} // namespace nexus::geometry
