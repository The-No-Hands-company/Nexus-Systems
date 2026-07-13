#include <nexus/geometry/HalfEdgeMesh.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// --- fromMesh ----------------------------------------------------------------

std::optional<HalfEdgeMesh> HalfEdgeMesh::fromMesh(const Mesh& mesh) {
    if (!mesh.isValid()) return std::nullopt;

    const auto& attrs = mesh.attributes();
    const auto& topo = mesh.topology();
    const size_t nFaces = topo.faceCount();
    const size_t nVerts = attrs.vertexCount();

    if (nFaces == 0 || nVerts == 0) return std::nullopt;
    if (!topo.allFacesArePoly3Plus()) return std::nullopt;
    if (!topo.hasValidIndices(nVerts)) return std::nullopt;

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() < 3) return std::nullopt;
        std::set<uint32_t> uniq(f.indices.begin(), f.indices.end());
        if (uniq.size() != f.indices.size()) return std::nullopt;
    }

    HalfEdgeMesh hem;
    hem.m_verts.resize(nVerts);
    hem.m_faces.resize(nFaces);
    hem.m_positions = attrs.positions();
    if (attrs.hasUVs()) hem.m_uvs = attrs.uvs();
    if (attrs.hasNormals()) hem.m_normals = attrs.normals();
    if (attrs.hasTangents()) hem.m_tangents = attrs.tangents();
    if (attrs.hasSkinning()) {
        hem.m_jointIndices = attrs.jointIndices();
        hem.m_jointWeights = attrs.jointWeights();
    }

    std::unordered_map<uint64_t, uint32_t> edgeMap;
    std::vector<std::vector<uint32_t>> faceEdges(nFaces);

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        const uint32_t n = static_cast<uint32_t>(f.indices.size());
        faceEdges[fi].reserve(n);

        for (uint32_t j = 0; j < n; ++j) {
            const uint32_t src = f.indices[j];
            const uint32_t dst = f.indices[(j + 1) % n];

            if (src >= nVerts || dst >= nVerts) return std::nullopt;

            uint32_t heIdx = static_cast<uint32_t>(hem.m_edges.size());
            HEEdge he;
            he.src = src;
            he.face = static_cast<uint32_t>(fi);
            hem.m_edges.push_back(he);
            faceEdges[fi].push_back(heIdx);

            if (hem.m_verts[src].edge == kInvalid) {
                hem.m_verts[src].edge = heIdx;
            }

            uint64_t revKey = (static_cast<uint64_t>(dst) << 32) | static_cast<uint64_t>(src);
            auto twinIt = edgeMap.find(revKey);
            if (twinIt != edgeMap.end()) {
                uint32_t twinIdx = twinIt->second;
                hem.m_edges[heIdx].twin = twinIdx;
                hem.m_edges[twinIdx].twin = heIdx;
                edgeMap.erase(twinIt);
            } else {
                uint64_t fwdKey = (static_cast<uint64_t>(src) << 32) | static_cast<uint64_t>(dst);
                edgeMap[fwdKey] = heIdx;
            }
        }

        for (uint32_t j = 0; j < n; ++j) {
            uint32_t heCurr = faceEdges[fi][j];
            uint32_t heNext = faceEdges[fi][(j + 1) % n];
            uint32_t hePrev = faceEdges[fi][(j + n - 1) % n];
            hem.m_edges[heCurr].next = heNext;
            hem.m_edges[heCurr].prev = hePrev;
        }

        hem.m_faces[fi].edge = faceEdges[fi][0];
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(hem.m_edges.size()); ++i) {
        hem.updateEdgeMap(i);
    }

    return hem;
}

// --- toMesh ------------------------------------------------------------------

Mesh HalfEdgeMesh::toMesh() const {
    Mesh mesh;
    const size_t nv = m_positions.size();
    mesh.attributes().setPositions(m_positions);
    // Emit each optional stream only when it is per-vertex consistent. Some
    // vertex-adding ops don't yet maintain every stream; emitting a desynced
    // buffer would corrupt the mesh, so guard on an exact size match rather
    // than mere non-emptiness.
    if (m_uvs.size() == nv) mesh.attributes().setUVs(m_uvs);
    if (m_normals.size() == nv) mesh.attributes().setNormals(m_normals);
    if (m_tangents.size() == nv) mesh.attributes().setTangents(m_tangents);
    if (m_jointIndices.size() == nv && m_jointWeights.size() == nv) {
        mesh.attributes().setSkinning(m_jointIndices, m_jointWeights);
    }

    for (uint32_t fi = 0; fi < static_cast<uint32_t>(m_faces.size()); ++fi) {
        std::vector<uint32_t> verts;
        uint32_t start = m_faces[fi].edge;
        if (start == kInvalid) continue;

        uint32_t e = start;
        do {
            verts.push_back(m_edges[e].src);
            e = m_edges[e].next;
        } while (e != start);

        for (size_t i = 2; i < verts.size(); ++i) {
            Face f;
            f.indices = {verts[0], verts[static_cast<uint32_t>(i) - 1], verts[i]};
            mesh.topology().addFace(std::move(f));
        }
    }

    return mesh;
}

// --- isManifold --------------------------------------------------------------

bool HalfEdgeMesh::isManifold() const {
    if (m_edges.empty()) return false;

    for (size_t i = 0; i < m_edges.size(); ++i) {
        const auto& e = m_edges[i];
        if (e.twin == kInvalid) return false;
        if (e.twin >= m_edges.size()) return false;
        if (m_edges[e.twin].twin != static_cast<uint32_t>(i)) return false;
    }

    std::vector<bool> edgeVisited(m_edges.size(), false);

    for (uint32_t v = 0; v < static_cast<uint32_t>(m_verts.size()); ++v) {
        uint32_t start = m_verts[v].edge;
        if (start == kInvalid) continue;

        uint32_t walk = start;
        uint32_t visited = 0;
        do {
            if (edgeVisited[walk]) break;
            edgeVisited[walk] = true;
            ++visited;
            uint32_t prevEdge = m_edges[walk].prev;
            if (prevEdge == kInvalid || prevEdge >= m_edges.size()) break;
            uint32_t prevTwin = m_edges[prevEdge].twin;
            if (prevTwin == kInvalid) break;
            walk = prevTwin;
        } while (walk != start && visited < m_edges.size());

        if (walk != start) return false;
    }

    return true;
}

// --- isClosed ----------------------------------------------------------------

bool HalfEdgeMesh::isClosed() const {
    for (const auto& e : m_edges) {
        if (e.twin == kInvalid) return false;
    }
    return !m_edges.empty();
}

// --- isTriangulated ----------------------------------------------------------

bool HalfEdgeMesh::isTriangulated() const {
    for (uint32_t fi = 0; fi < static_cast<uint32_t>(m_faces.size()); ++fi) {
        uint32_t start = m_faces[fi].edge;
        if (start == kInvalid) return false;

        uint32_t e = start;
        uint32_t count = 0;
        do {
            ++count;
            e = m_edges[e].next;
            if (count > 100) return false;
        } while (e != start);

        if (count != 3) return false;
    }
    return !m_faces.empty();
}

// --- checkIntegrity ----------------------------------------------------------
//  Direct validation of the half-edge invariants over the *live* sub-complex
//  (edges/faces are tombstoned in-place by the Euler ops rather than compacted,
//  so we must skip the dead ones and — crucially — verify that no live element
//  ever references a dead one). Reports the first violation for debuggability.

HalfEdgeMesh::IntegrityReport HalfEdgeMesh::checkIntegrity() const {
    IntegrityReport r;
    const uint32_t E = static_cast<uint32_t>(m_edges.size());
    const uint32_t V = static_cast<uint32_t>(m_verts.size());
    const uint32_t F = static_cast<uint32_t>(m_faces.size());

    auto fail = [&](std::string why) -> IntegrityReport {
        IntegrityReport bad;
        bad.ok = false;
        bad.reason = std::move(why);
        return bad;
    };
    auto edgeLive = [&](uint32_t e) -> bool {
        return e < E && m_edges[e].face != kInvalid;
    };

    std::vector<bool> vertUsed(V, false);

    for (uint32_t e = 0; e < E; ++e) {
        const auto& he = m_edges[e];
        if (he.face == kInvalid) continue;  // tombstoned — dead half-edge
        ++r.liveEdges;

        // face back-reference must be a live face
        if (he.face >= F || m_faces[he.face].edge == kInvalid)
            return fail("live edge " + std::to_string(e) + " points to dead/invalid face");

        // next / prev must be live and mutually inverse, sharing this face
        if (!edgeLive(he.next))
            return fail("live edge " + std::to_string(e) + " has dead/invalid next");
        if (!edgeLive(he.prev))
            return fail("live edge " + std::to_string(e) + " has dead/invalid prev");
        if (m_edges[he.next].prev != e)
            return fail("next.prev mismatch at edge " + std::to_string(e));
        if (m_edges[he.prev].next != e)
            return fail("prev.next mismatch at edge " + std::to_string(e));
        if (m_edges[he.next].face != he.face || m_edges[he.prev].face != he.face)
            return fail("face-cycle spans multiple faces at edge " + std::to_string(e));

        // src must be a valid vertex
        if (he.src >= V)
            return fail("live edge " + std::to_string(e) + " has invalid src");
        vertUsed[he.src] = true;

        // twin: either a boundary (kInvalid) or a live, reciprocal, opposite-face
        // half-edge whose src is this edge's destination.
        if (he.twin == kInvalid) {
            ++r.boundaryEdges;
        } else {
            if (!edgeLive(he.twin))
                return fail("live edge " + std::to_string(e) + " has dead/invalid twin");
            if (m_edges[he.twin].twin != e)
                return fail("twin is not reciprocal at edge " + std::to_string(e));
            if (m_edges[he.twin].face == he.face)
                return fail("twin shares face (non-manifold fold) at edge " + std::to_string(e));
            if (m_edges[he.twin].src != m_edges[he.next].src)
                return fail("twin.src != edge destination at edge " + std::to_string(e));
        }
    }

    // Every live face's edge points back to a live half-edge on that face,
    // and its cycle is a bounded simple loop.
    for (uint32_t f = 0; f < F; ++f) {
        if (m_faces[f].edge == kInvalid) continue;  // tombstoned — dead face
        ++r.liveFaces;
        uint32_t start = m_faces[f].edge;
        if (!edgeLive(start) || m_edges[start].face != f)
            return fail("face " + std::to_string(f) + " edge does not belong to it");
        uint32_t walk = start, steps = 0;
        do {
            if (m_edges[walk].face != f)
                return fail("face " + std::to_string(f) + " cycle leaves the face");
            walk = m_edges[walk].next;
            if (++steps > E)
                return fail("face " + std::to_string(f) + " cycle does not close");
        } while (walk != start);
        if (steps < 3)
            return fail("face " + std::to_string(f) + " has fewer than 3 sides");
    }

    // Vertex edge references, when set, must point at a live half-edge rooted here.
    for (uint32_t v = 0; v < V; ++v) {
        uint32_t ve = m_verts[v].edge;
        if (ve == kInvalid) continue;
        if (!edgeLive(ve))
            return fail("vertex " + std::to_string(v) + " points to a dead/invalid edge");
        if (m_edges[ve].src != v)
            return fail("vertex " + std::to_string(v) + " edge is not rooted at it");
    }

    for (bool used : vertUsed)
        if (used) ++r.liveVerts;

    return r;
}

// --- boundaryLoops -----------------------------------------------------------

std::vector<std::vector<uint32_t>> HalfEdgeMesh::boundaryLoops() const {
    std::vector<std::vector<uint32_t>> loops;
    std::vector<bool> visited(m_edges.size(), false);

    for (uint32_t i = 0; i < static_cast<uint32_t>(m_edges.size()); ++i) {
        if (m_edges[i].twin != kInvalid) continue;
        if (visited[i]) continue;

        std::vector<uint32_t> loop;
        uint32_t e = i;
        uint32_t safety = 0;
        do {
            if (++safety > m_edges.size() * 3) break;
            visited[e] = true;
            loop.push_back(m_edges[e].src);

            uint32_t nxt = m_edges[e].next;
            if (nxt == kInvalid || nxt >= m_edges.size()) break;
            e = nxt;
            uint32_t twinSafety = 0;
            while (m_edges[e].twin != kInvalid) {
                if (++twinSafety > m_edges.size()) break;
                uint32_t tw = m_edges[e].twin;
                if (tw == kInvalid || tw >= m_edges.size()) break;
                uint32_t tnxt = m_edges[tw].next;
                if (tnxt == kInvalid || tnxt >= m_edges.size()) break;
                e = tnxt;
            }
        } while (e != i);

        loops.push_back(std::move(loop));
    }

    return loops;
}

// --- findEdge ----------------------------------------------------------------

uint32_t HalfEdgeMesh::findEdge(uint32_t src, uint32_t dst) const {
    uint64_t key = (static_cast<uint64_t>(src) << 32) | static_cast<uint64_t>(dst);
    auto it = m_edgeMap.find(key);
    if (it != m_edgeMap.end()) {
        uint32_t ei = it->second;
        const auto& e = m_edges[ei];
        if (e.face != kInvalid && e.next != kInvalid && m_edges[e.next].src == dst) {
            return ei;
        }
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_edges.size()); ++i) {
        if (m_edges[i].src != src) continue;
        if (m_edges[i].face == kInvalid) continue;
        if (m_edges[i].next == kInvalid) continue;
        if (m_edges[m_edges[i].next].src == dst) return i;
    }
    return kInvalid;
}

// --- vertexFaces -------------------------------------------------------------

std::vector<uint32_t> HalfEdgeMesh::vertexFaces(uint32_t v) const {
    std::vector<uint32_t> faces;
    if (v >= m_verts.size()) return faces;

    uint32_t start = m_verts[v].edge;
    if (start == kInvalid) return faces;

    uint32_t e = start;
    uint32_t safety = 0;
    do {
        if (m_edges[e].face != kInvalid) {
            faces.push_back(m_edges[e].face);
        }
        uint32_t prevEdge = m_edges[e].prev;
        if (prevEdge == kInvalid || prevEdge >= m_edges.size()) break;
        uint32_t prevTwin = m_edges[prevEdge].twin;
        if (prevTwin == kInvalid) break;
        e = prevTwin;
        if (++safety > m_edges.size() * 2) break;
    } while (e != start);

    return faces;
}

// --- vertexNeighbors ---------------------------------------------------------

std::vector<uint32_t> HalfEdgeMesh::vertexNeighbors(uint32_t v) const {
    std::vector<uint32_t> neighbors;
    if (v >= m_verts.size()) return neighbors;

    uint32_t start = m_verts[v].edge;
    if (start == kInvalid) return neighbors;

    uint32_t e = start;
    uint32_t safety = 0;
    do {
        uint32_t nextEdge = m_edges[e].next;
        if (nextEdge == kInvalid || nextEdge >= m_edges.size()) break;
        neighbors.push_back(m_edges[nextEdge].src);

        uint32_t prevEdge = m_edges[e].prev;
        if (prevEdge == kInvalid || prevEdge >= m_edges.size()) break;
        uint32_t prevTwin = m_edges[prevEdge].twin;
        if (prevTwin == kInvalid) break;
        e = prevTwin;
        if (++safety > m_edges.size() * 2) break;
    } while (e != start);

    return neighbors;
}

// --- faceValence ------------------------------------------------------------

uint32_t HalfEdgeMesh::faceValence(uint32_t fi) const {
    if (fi >= m_faces.size()) return 0;
    uint32_t start = m_faces[fi].edge;
    if (start == kInvalid || start >= m_edges.size()) return 0;
    uint32_t count = 0;
    uint32_t e = start;
    do {
        ++count;
        uint32_t nextEdge = m_edges[e].next;
        if (nextEdge == kInvalid || nextEdge >= m_edges.size()) break;
        e = nextEdge;
        if (count > 256) break;
    } while (e != start);
    return count;
}

// --- edgeValence ------------------------------------------------------------

uint32_t HalfEdgeMesh::edgeValence(uint32_t ei) const {
    if (ei >= m_edges.size()) return 0;
    uint32_t count = 0;
    if (m_edges[ei].face != kInvalid) ++count;
    uint32_t t = m_edges[ei].twin;
    if (t != kInvalid && t < m_edges.size() && m_edges[t].face != kInvalid) ++count;
    return count;
}

// --- isBoundaryEdge ---------------------------------------------------------

bool HalfEdgeMesh::isBoundaryEdge(uint32_t ei) const {
    if (ei >= m_edges.size()) return false;
    return m_edges[ei].twin == kInvalid;
}

// --- isBoundaryVertex -------------------------------------------------------

bool HalfEdgeMesh::isBoundaryVertex(uint32_t v) const {
    if (v >= m_verts.size()) return false;
    uint32_t start = m_verts[v].edge;
    if (start == kInvalid) return true;
    uint32_t e = start;
    uint32_t safety = 0;
    do {
        if (m_edges[e].twin == kInvalid) return true;
        uint32_t prevEdge = m_edges[e].prev;
        if (prevEdge == kInvalid || prevEdge >= m_edges.size()) break;
        uint32_t prevTwin = m_edges[prevEdge].twin;
        if (prevTwin == kInvalid) break;
        e = prevTwin;
        if (++safety > m_edges.size()) break;
    } while (e != start);
    return false;
}

// --- edgeLoop ---------------------------------------------------------------

std::vector<uint32_t> HalfEdgeMesh::edgeLoop(uint32_t seedEdge) const {
    std::vector<uint32_t> loop;
    if (seedEdge >= m_edges.size()) return loop;
    if (m_edges[seedEdge].face == kInvalid) return loop;

    std::unordered_set<uint32_t> visited;
    loop.push_back(seedEdge);
    visited.insert(seedEdge);

    uint32_t maxSteps = static_cast<uint32_t>(m_edges.size());

    auto walkOneDir = [&](uint32_t start, bool forward) {
        uint32_t e = start;
        for (uint32_t step = 0; step < maxSteps; ++step) {
            uint32_t f = m_edges[e].face;
            if (f == kInvalid || f >= m_faces.size()) break;

            uint32_t valence = faceValence(f);
            if (valence < 3 || valence > 4) break;

            uint32_t cand = kInvalid;
            if (forward) {
                cand = m_edges[m_edges[e].next].next;
            } else {
                cand = m_edges[m_edges[e].prev].prev;
            }
            if (cand == kInvalid || cand >= m_edges.size()) break;

            uint32_t opp = m_edges[cand].twin;
            if (opp == kInvalid || opp >= m_edges.size()) break;
            if (m_edges[opp].face == kInvalid) break;

            if (visited.count(opp)) break;
            visited.insert(opp);
            if (forward) loop.push_back(opp);
            else loop.insert(loop.begin(), opp);
            e = opp;
        }
    };

    walkOneDir(seedEdge, true);
    walkOneDir(seedEdge, false);

    return loop;
}

// --- edgeRing ---------------------------------------------------------------

std::vector<uint32_t> HalfEdgeMesh::edgeRing(uint32_t seedEdge) const {
    std::vector<uint32_t> ring;
    if (seedEdge >= m_edges.size()) return ring;

    uint32_t t = m_edges[seedEdge].twin;
    if (t == kInvalid || t >= m_edges.size()) return ring;

    std::unordered_set<uint32_t> visited;
    visited.insert(seedEdge);
    visited.insert(t);
    ring.push_back(seedEdge);

    uint32_t maxSteps = static_cast<uint32_t>(m_edges.size());

    auto walkOneDir = [&](uint32_t start, bool forward) {
        uint32_t e = start;
        for (uint32_t step = 0; step < maxSteps; ++step) {
            uint32_t f = m_edges[e].face;
            if (f == kInvalid || f >= m_faces.size()) break;

            uint32_t valence = faceValence(f);
            if (valence < 3 || valence > 4) break;

            uint32_t cand = forward ? m_edges[e].next : m_edges[e].prev;
            if (cand == kInvalid || cand >= m_edges.size()) break;

            uint32_t nxt = m_edges[cand].twin;
            if (nxt == kInvalid || nxt >= m_edges.size()) break;
            if (m_edges[nxt].face == kInvalid) break;

            if (visited.count(nxt)) break;
            visited.insert(nxt);
            ring.push_back(nxt);
            e = nxt;
        }
    };

    walkOneDir(t, true);
    std::reverse(ring.begin() + 1, ring.end());

    walkOneDir(seedEdge, false);
    std::reverse(ring.begin() + 1, ring.end());

    return ring;
}

// --- insertEdgeLoop ---------------------------------------------------------
//  Inserts an edge loop at 'slide' position and splits crossed faces.
//  Creates new vertices on each loop edge, then adds cross-edges to split quads.

bool HalfEdgeMesh::insertEdgeLoop(uint32_t seedEdge, float slide) {
    if (seedEdge >= m_edges.size()) return false;
    if (m_edges[seedEdge].face == kInvalid) return false;

    auto loop = edgeLoop(seedEdge);
    if (loop.size() < 2) return false;

    slide = std::clamp(slide, 0.001f, 0.999f);

    std::vector<uint32_t> newVerts;
    std::vector<uint32_t> splitEdges;
    newVerts.reserve(loop.size());
    splitEdges.reserve(loop.size());

    for (uint32_t ei : loop) {
        if (ei >= m_edges.size()) return false;
        uint32_t t = m_edges[ei].twin;
        if (t == kInvalid || t >= m_edges.size()) continue;

        uint32_t src = m_edges[ei].src;
        uint32_t dst = m_edges[t].src;
        if (src >= m_verts.size() || dst >= m_verts.size()) continue;

        Vec3 newPos{
            m_positions[src].x + (m_positions[dst].x - m_positions[src].x) * slide,
            m_positions[src].y + (m_positions[dst].y - m_positions[src].y) * slide,
            m_positions[src].z + (m_positions[dst].z - m_positions[src].z) * slide,
        };
        uint32_t nv = static_cast<uint32_t>(m_verts.size());
        m_verts.push_back({kInvalid});
        m_positions.push_back(newPos);
        if (hasUVs()) {
            m_uvs.push_back({m_uvs[src].u + (m_uvs[dst].u - m_uvs[src].u) * slide,
                             m_uvs[src].v + (m_uvs[dst].v - m_uvs[src].v) * slide});
        }
        if (hasNormals()) {
            m_normals.push_back({(m_normals[src].x + m_normals[dst].x) * 0.5f,
                                 (m_normals[src].y + m_normals[dst].y) * 0.5f,
                                 (m_normals[src].z + m_normals[dst].z) * 0.5f});
        }
        newVerts.push_back(nv);
        splitEdges.push_back(ei);
    }

    for (size_t i = 0; i < loop.size(); ++i) {
        uint32_t ei = splitEdges[i];
        uint32_t nv = newVerts[i];
        uint32_t t = m_edges[ei].twin;
        if (ei >= m_edges.size() || t >= m_edges.size()) continue;

        uint32_t fA = m_edges[ei].face;
        uint32_t fB = m_edges[t].face;
        uint32_t srcV = m_edges[ei].src;   // ei: srcV -> dstV
        uint32_t dstV = m_edges[t].src;    // t : dstV -> srcV

        uint32_t e1 = static_cast<uint32_t>(m_edges.size());
        m_edges.push_back({}); m_edges.push_back({});

        uint32_t oldNextA = m_edges[ei].next;
        uint32_t oldPrevA = m_edges[ei].prev;
        if (oldNextA >= m_edges.size() || oldPrevA >= m_edges.size()) continue;

        m_edges[e1] = {e1+1, oldPrevA, kInvalid, srcV, fA};   // srcV -> nv
        m_edges[e1+1] = {oldNextA, e1, kInvalid, nv, fA};      // nv   -> dstV
        if (oldPrevA < m_edges.size()) m_edges[oldPrevA].next = e1;
        if (oldNextA < m_edges.size()) m_edges[oldNextA].prev = e1+1;

        uint32_t e2 = static_cast<uint32_t>(m_edges.size());
        m_edges.push_back({}); m_edges.push_back({});

        uint32_t oldNextB = m_edges[t].next;
        uint32_t oldPrevB = m_edges[t].prev;
        if (oldNextB >= m_edges.size() || oldPrevB >= m_edges.size()) continue;

        // Twin pairing must match directions: e1 (srcV->nv) opposes e2+1
        // (nv->srcV), and e1+1 (nv->dstV) opposes e2 (dstV->nv). NOT the crossed
        // e1<->e2 / e1+1<->e2+1 pairing (same bug class as insertEdgeVertex).
        m_edges[e2] = {e2+1, oldPrevB, e1+1, dstV, fB};   // dstV -> nv
        m_edges[e2+1] = {oldNextB, e2, e1, nv, fB};        // nv   -> srcV
        m_edges[e1].twin = e2+1;
        m_edges[e1+1].twin = e2;

        if (oldPrevB < m_edges.size()) m_edges[oldPrevB].next = e2;
        if (oldNextB < m_edges.size()) m_edges[oldNextB].prev = e2+1;

        m_edges[ei].face = kInvalid; m_edges[ei].next = kInvalid;
        m_edges[ei].prev = kInvalid; m_edges[ei].twin = kInvalid;
        m_edges[t].face = kInvalid; m_edges[t].next = kInvalid;
        m_edges[t].prev = kInvalid; m_edges[t].twin = kInvalid;

        if (fA < m_faces.size()) m_faces[fA].edge = e1;
        if (fB < m_faces.size()) m_faces[fB].edge = e2;
        m_verts[nv].edge = e1+1;
        // Re-root the split endpoints off the now-dead ei / t.
        if (srcV < m_verts.size() && m_verts[srcV].edge == ei) m_verts[srcV].edge = e1;
        if (dstV < m_verts.size() && m_verts[dstV].edge == t) m_verts[dstV].edge = e2;

        updateEdgeMap(e1); updateEdgeMap(e1+1);
        updateEdgeMap(e2); updateEdgeMap(e2+1);
    }

    // Phase 3: Connect consecutive new vertices with cross edges,
    // splitting each crossed face along the loop.
    splitFacesAlongLoop(newVerts);

    return true;
}

// --- splitFacesAlongLoop -----------------------------------------------------
//  After inserting new vertices along an edge loop, adds cross-edges
//  connecting consecutive new vertices that share a face.
//  Each crossed face is split into two faces along the cross-edge.

bool HalfEdgeMesh::splitFacesAlongLoop(const std::vector<uint32_t>& newVerts) {
    if (newVerts.size() < 2) return false;

    // Each consecutive pair of loop vertices shares one crossed face; splitting
    // that face along the diagonal is exactly connectVertices (which properly
    // creates two faces with cross edges wired into their cycles). The previous
    // implementation added a dangling, un-wired edge pair via addEdgePair and
    // left the mesh integrity-broken.
    bool any = false;
    for (size_t i = 0; i < newVerts.size(); ++i) {
        uint32_t vA = newVerts[i];
        uint32_t vB = newVerts[(i + 1) % newVerts.size()];
        if (vA == vB) continue;
        if (connectVertices(vA, vB)) any = true;
    }
    return any;
}

// --- slideEdgeLoop ----------------------------------------------------------

bool HalfEdgeMesh::slideEdgeLoop(uint32_t seedEdge, float slide) {
    if (seedEdge >= m_edges.size()) return false;
    if (m_edges[seedEdge].face == kInvalid) return false;

    auto loop = edgeLoop(seedEdge);
    if (loop.empty()) return false;

    slide = std::clamp(slide, 0.0f, 1.0f);

    for (uint32_t ei : loop) {
        uint32_t t = m_edges[ei].twin;
        if (t == kInvalid || t >= m_edges.size()) continue;
        uint32_t src = m_edges[ei].src;
        uint32_t dst = m_edges[t].src;
        if (src >= m_verts.size() || dst >= m_verts.size()) continue;

        m_positions[src].x = m_positions[src].x + (m_positions[dst].x - m_positions[src].x) * slide;
        m_positions[src].y = m_positions[src].y + (m_positions[dst].y - m_positions[src].y) * slide;
        m_positions[src].z = m_positions[src].z + (m_positions[dst].z - m_positions[src].z) * slide;
    }

    return true;
}

// --- flipEdge ----------------------------------------------------------------

bool HalfEdgeMesh::flipEdge(uint32_t he) {
    if (he >= m_edges.size()) return false;

    uint32_t t = m_edges[he].twin;
    if (t == kInvalid || t >= m_edges.size()) return false;

    uint32_t a = he;
    uint32_t b = m_edges[a].next;
    uint32_t c = m_edges[a].prev;

    uint32_t d = t;
    uint32_t e2 = m_edges[d].next;
    uint32_t f2 = m_edges[d].prev;

    uint32_t v0 = m_edges[a].src;
    uint32_t v1 = m_edges[b].src;
    uint32_t v2 = m_edges[c].src;
    uint32_t v3 = m_edges[f2].src;

    uint32_t countA = faceValence(m_edges[a].face);
    uint32_t countB = faceValence(m_edges[d].face);
    if (countA != 3 || countB != 3) return false;

    if (findEdge(v2, v3) != kInvalid || findEdge(v3, v2) != kInvalid) return false;

    uint32_t fA = m_edges[a].face;
    uint32_t fB = m_edges[d].face;

    m_edges[a].src = v2;
    m_edges[d].src = v3;

    m_edges[d].next = c;   m_edges[d].prev = e2;   m_edges[d].face = fA;
    m_edges[c].next = e2;   m_edges[c].prev = d;   m_edges[c].face = fA;
    m_edges[e2].next = d;   m_edges[e2].prev = c;   m_edges[e2].face = fA;

    m_edges[a].next = f2;   m_edges[a].prev = b;   m_edges[a].face = fB;
    m_edges[f2].next = b;   m_edges[f2].prev = a;   m_edges[f2].face = fB;
    m_edges[b].next = a;   m_edges[b].prev = f2;   m_edges[b].face = fB;

    m_edges[a].twin = d;
    m_edges[d].twin = a;

    m_faces[fA].edge = d;
    m_faces[fB].edge = a;

    if (m_verts[v0].edge == a || m_verts[v0].edge == c) m_verts[v0].edge = e2;
    if (m_verts[v1].edge == b || m_verts[v1].edge == d) m_verts[v1].edge = b;
    if (m_verts[v2].edge == c) m_verts[v2].edge = a;
    if (m_verts[v3].edge == f2) m_verts[v3].edge = d;

    updateEdgeMap(a);
    updateEdgeMap(d);

    return true;
}

// --- splitEdge ---------------------------------------------------------------
//  Splits an edge and both incident faces. Works on triangles only.
//  Creates 1 new vertex (midpoint) and 4 new triangle faces from 2 original triangles.

bool HalfEdgeMesh::splitEdge(uint32_t he) {
    if (he >= m_edges.size()) return false;

    uint32_t t = m_edges[he].twin;
    if (t == kInvalid || t >= m_edges.size()) return false;

    uint32_t a = he;
    uint32_t b = m_edges[a].next;
    uint32_t c = m_edges[a].prev;

    uint32_t d = t;
    uint32_t e2 = m_edges[d].next;
    uint32_t f2 = m_edges[d].prev;

    uint32_t v0 = m_edges[a].src;
    uint32_t v1 = m_edges[b].src;
    uint32_t v2 = m_edges[c].src;
    uint32_t v3 = m_edges[f2].src;

    uint32_t countA = faceValence(m_edges[a].face);
    uint32_t countB = faceValence(m_edges[d].face);

    // For non-triangle faces, use insertEdgeVertex (edge subdivision without face split)
    if (countA != 3 || countB != 3) {
        return insertEdgeVertex(he, 0.5f);
    }

    const auto& p0 = m_positions[v0];
    const auto& p1 = m_positions[v1];
    Vec3 mid{(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f, (p0.z + p1.z) * 0.5f};
    uint32_t vMid = static_cast<uint32_t>(m_positions.size());
    m_positions.push_back(mid);
    m_verts.push_back({});

    uint32_t fA = m_edges[a].face;
    uint32_t fB = m_edges[d].face;

    uint32_t fA0 = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});
    uint32_t fA1 = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});
    uint32_t fB0 = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});
    uint32_t fB1 = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});

    uint32_t eA0_0 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t eA0_1 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t etA0_0 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t etA0_1 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t eA1_0 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t etA1_0 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t eB0_1 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});
    uint32_t etB0_1 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({});

    m_edges[eA0_0] = {eA0_1, c, etA0_0, v0, fA0};
    m_edges[eA0_1] = {c, eA0_0, etA0_1, vMid, fA0};
    m_edges[c].next = eA0_0; m_edges[c].prev = eA0_1; m_edges[c].face = fA0;

    // fB1 cycle is etA0_0 → e2 → etB0_1 → etA0_0, so etA0_0's predecessor is
    // etB0_1 (not eB0_1, which lives on face fB0).
    m_edges[etA0_0] = {e2, etB0_1, eA0_0, vMid, fB1};
    m_edges[etA0_1] = {eA1_0, b, eA0_1, v2, fA1};

    m_edges[eA1_0] = {b, etA0_1, etA1_0, vMid, fA1};
    m_edges[b].next = etA0_1; m_edges[b].prev = eA1_0; m_edges[b].face = fA1;

    m_edges[etA1_0] = {eB0_1, f2, eA1_0, v1, fB0};

    m_edges[eB0_1] = {f2, etA1_0, etB0_1, vMid, fB0};
    m_edges[f2].next = etA1_0; m_edges[f2].prev = eB0_1; m_edges[f2].face = fB0;

    m_edges[etB0_1] = {etA0_0, e2, eB0_1, v3, fB1};
    m_edges[e2].next = etB0_1; m_edges[e2].prev = etA0_0; m_edges[e2].face = fB1;

    m_faces[fA].edge = kInvalid;
    m_faces[fB].edge = kInvalid;

    m_faces[fA0].edge = eA0_0;
    m_faces[fA1].edge = eA1_0;
    m_faces[fB0].edge = etA1_0;
    m_faces[fB1].edge = etA0_0;

    m_edges[a].face = kInvalid;
    m_edges[d].face = kInvalid;

    m_verts[vMid].edge = eA0_1;
    if (m_verts[v0].edge == a) m_verts[v0].edge = eA0_0;
    if (m_verts[v1].edge == d || m_verts[v1].edge == b) m_verts[v1].edge = etA1_0;

    updateEdgeMap(eA0_0); updateEdgeMap(eA0_1);
    updateEdgeMap(etA0_0); updateEdgeMap(etA0_1);
    updateEdgeMap(eA1_0); updateEdgeMap(etA1_0);
    updateEdgeMap(eB0_1); updateEdgeMap(etB0_1);

    return true;
}

// --- insertEdgeVertex --------------------------------------------------------
//  Inserts a new vertex along an edge at parametric position t (0=src, 1=dst).
//  Works on arbitrary valence faces (triangles, quads, NGons).
//  Does NOT create new faces — inserts vertex into existing face edge cycles.

bool HalfEdgeMesh::insertEdgeVertex(uint32_t he, float t) {
    if (he >= m_edges.size()) return false;

    uint32_t twin = m_edges[he].twin;
    uint32_t vSrc = m_edges[he].src;

    uint32_t vDst = kInvalid;
    if (twin != kInvalid && twin < m_edges.size()) {
        vDst = m_edges[twin].src;
    } else if (m_edges[he].next != kInvalid && m_edges[he].next < m_edges.size()) {
        vDst = m_edges[m_edges[he].next].src;
    }
    if (vDst == kInvalid || vSrc >= m_verts.size() || vDst >= m_verts.size()) return false;

    bool hasTwin = (twin != kInvalid && twin < m_edges.size());

    uint32_t fA = m_edges[he].face;
    uint32_t fB = hasTwin ? m_edges[twin].face : kInvalid;

    t = std::clamp(t, 0.001f, 0.999f);
    Vec3 newPos{
        m_positions[vSrc].x + (m_positions[vDst].x - m_positions[vSrc].x) * t,
        m_positions[vSrc].y + (m_positions[vDst].y - m_positions[vSrc].y) * t,
        m_positions[vSrc].z + (m_positions[vDst].z - m_positions[vSrc].z) * t,
    };
    uint32_t vMid = static_cast<uint32_t>(m_positions.size());
    m_positions.push_back(newPos);
    m_verts.push_back({kInvalid});
    if (hasUVs()) {
        m_uvs.push_back({m_uvs[vSrc].u + (m_uvs[vDst].u - m_uvs[vSrc].u) * t,
                         m_uvs[vSrc].v + (m_uvs[vDst].v - m_uvs[vSrc].v) * t});
    }
    if (hasNormals()) {
        m_normals.push_back({(m_normals[vSrc].x + m_normals[vDst].x) * 0.5f,
                             (m_normals[vSrc].y + m_normals[vDst].y) * 0.5f,
                             (m_normals[vSrc].z + m_normals[vDst].z) * 0.5f});
    }

    // Create half-edges: src->vMid, vMid->nextA, and optionally twins
    uint32_t e0 = static_cast<uint32_t>(m_edges.size());
    m_edges.push_back({}); m_edges.push_back({});
    if (hasTwin) { m_edges.push_back({}); m_edges.push_back({}); }

    // Wire face A
    uint32_t oldNextA = m_edges[he].next;
    uint32_t oldPrevA = m_edges[he].prev;
    if (oldNextA >= m_edges.size() || oldPrevA >= m_edges.size()) {
        m_edges.resize(e0);
        m_positions.pop_back();
        m_verts.pop_back();
        return false;
    }

    // Twin pairing must match half-edge directions: e0 is (vSrc→vMid) and
    // e0+1 is (vMid→vDst); their opposites on face B are e0+3 (vMid→vSrc) and
    // e0+2 (vDst→vMid) respectively — NOT the crossed e0+2/e0+3 pairing.
    m_edges[e0] = {e0 + 1, oldPrevA, hasTwin ? e0 + 3 : kInvalid, vSrc, fA};
    m_edges[e0 + 1] = {oldNextA, e0, hasTwin ? e0 + 2 : kInvalid, vMid, fA};
    if (oldPrevA < m_edges.size()) m_edges[oldPrevA].next = e0;
    if (oldNextA < m_edges.size()) m_edges[oldNextA].prev = e0 + 1;

    if (hasTwin) {
        // Wire face B
        uint32_t oldNextB = m_edges[twin].next;
        uint32_t oldPrevB = m_edges[twin].prev;
        if (oldNextB >= m_edges.size() || oldPrevB >= m_edges.size()) {
            m_edges.resize(e0);
            m_positions.pop_back();
            m_verts.pop_back();
            return false;
        }

        m_edges[e0 + 2] = {e0 + 3, oldPrevB, e0 + 1, vDst, fB};
        m_edges[e0 + 3] = {oldNextB, e0 + 2, e0, vMid, fB};
        if (oldPrevB < m_edges.size()) m_edges[oldPrevB].next = e0 + 2;
        if (oldNextB < m_edges.size()) m_edges[oldNextB].prev = e0 + 3;

        // Destroy twin
        m_edges[twin].face = kInvalid;
        m_edges[twin].next = kInvalid;
        m_edges[twin].prev = kInvalid;
        m_edges[twin].twin = kInvalid;

        if (fB < m_faces.size()) m_faces[fB].edge = (m_faces[fB].edge == twin || m_faces[fB].edge >= m_edges.size()) ? e0 + 2 : m_faces[fB].edge;
    }

    // Destroy old edge
    m_edges[he].face = kInvalid;
    m_edges[he].next = kInvalid;
    m_edges[he].prev = kInvalid;
    m_edges[he].twin = kInvalid;

    if (fA < m_faces.size()) m_faces[fA].edge = (m_faces[fA].edge == he || m_faces[fA].edge >= m_edges.size()) ? e0 : m_faces[fA].edge;

    m_verts[vMid].edge = e0 + 1;

    // Re-root the endpoints if they referenced the now-dead original edges:
    // e0 is rooted at vSrc, e0+2 (face B) is rooted at vDst.
    if (m_verts[vSrc].edge == he) m_verts[vSrc].edge = e0;
    if (hasTwin && vDst < m_verts.size() && m_verts[vDst].edge == twin) m_verts[vDst].edge = e0 + 2;

    updateEdgeMap(e0); updateEdgeMap(e0 + 1);
    if (hasTwin) { updateEdgeMap(e0 + 2); updateEdgeMap(e0 + 3); }

    return true;
}


// --- collapseEdge ------------------------------------------------------------

bool HalfEdgeMesh::collapseEdge(uint32_t he, const nexus::render::Vec3& target) {
    if (he >= m_edges.size()) return false;

    uint32_t t = m_edges[he].twin;
    if (t == kInvalid || t >= m_edges.size()) return false;

    uint32_t a = he;
    uint32_t b = m_edges[a].next;
    uint32_t c = m_edges[a].prev;

    uint32_t d = t;
    uint32_t e = m_edges[d].next;
    uint32_t f = m_edges[d].prev;

    uint32_t vKeep = m_edges[d].src;
    uint32_t vRemove = m_edges[a].src;
    uint32_t vl = m_edges[c].src;
    uint32_t vr = m_edges[f].src;

    if (vKeep == vRemove) return false;

    // This collapse assumes both incident faces are triangles (it stitches the
    // two wing edges of each). Reject non-triangle faces rather than corrupt them.
    if (faceValence(m_edges[a].face) != 3 || faceValence(m_edges[d].face) != 3) return false;

    std::unordered_set<uint32_t> neighborsRemove;
    {
        uint32_t start = m_verts[vRemove].edge;
        if (start == kInvalid) return false;
        uint32_t walk = start;
        do {
            uint32_t nextEdge = m_edges[walk].next;
            if (nextEdge == kInvalid || nextEdge >= m_edges.size()) break;
            neighborsRemove.insert(m_edges[nextEdge].src);
            uint32_t prevEdge = m_edges[walk].prev;
            if (prevEdge == kInvalid || prevEdge >= m_edges.size()) break;
            uint32_t prevTwin = m_edges[prevEdge].twin;
            if (prevTwin == kInvalid) break;
            walk = prevTwin;
        } while (walk != start);
    }

    std::unordered_set<uint32_t> neighborsKeep;
    {
        uint32_t start = m_verts[vKeep].edge;
        if (start == kInvalid) return false;
        uint32_t walk = start;
        do {
            uint32_t nextEdge = m_edges[walk].next;
            if (nextEdge == kInvalid || nextEdge >= m_edges.size()) break;
            neighborsKeep.insert(m_edges[nextEdge].src);
            uint32_t prevEdge = m_edges[walk].prev;
            if (prevEdge == kInvalid || prevEdge >= m_edges.size()) break;
            uint32_t prevTwin = m_edges[prevEdge].twin;
            if (prevTwin == kInvalid) break;
            walk = prevTwin;
        } while (walk != start);
    }

    for (uint32_t n : neighborsRemove) {
        if (n == vl || n == vr) continue;
        if (neighborsKeep.count(n)) return false;
    }
    for (uint32_t n : neighborsKeep) {
        if (n == vl || n == vr) continue;
        if (neighborsRemove.count(n)) return false;
    }

    m_positions[vKeep] = target;

    // Save face indices before killEdge destroys them.
    uint32_t faceA = m_edges[he].face;
    uint32_t faceB = m_edges[t].face;

    for (auto& edge : m_edges) {
        if (edge.src == vRemove) edge.src = vKeep;
    }

    // Rebuild edgeMap entries for every surviving edge whose src changed.
    for (uint32_t ei = 0; ei < static_cast<uint32_t>(m_edges.size()); ++ei) {
        updateEdgeMap(ei);
    }

    // Capture the *outer* twins of the four wing edges before we destroy them.
    // Collapsing triangle A merges edges b and c onto the surviving (vKeep,vl)
    // wing: their outer neighbours (bo, co) become each other's twins. Likewise
    // (eo, fo) for triangle B. Without this rebond the survivors keep pointing
    // at now-dead half-edges (the "dead twin" integrity violation).
    const uint32_t bo = m_edges[b].twin;
    const uint32_t co = m_edges[c].twin;
    const uint32_t eo = m_edges[e].twin;
    const uint32_t fo = m_edges[f].twin;

    auto killEdge = [&](uint32_t ei) {
        if (ei < m_edges.size()) {
            m_edges[ei].face = kInvalid;
            m_edges[ei].next = kInvalid;
            m_edges[ei].prev = kInvalid;
            m_edges[ei].twin = kInvalid;
            m_edges[ei].src = kInvalid;
        }
    };

    killEdge(a); killEdge(b); killEdge(c);
    killEdge(d); killEdge(e); killEdge(f);

    // Rebond wing survivors (or open a boundary if one side had no outer twin).
    auto rebond = [&](uint32_t x, uint32_t y) {
        const bool lx = (x != kInvalid && x < m_edges.size());
        const bool ly = (y != kInvalid && y < m_edges.size());
        if (lx && ly) { m_edges[x].twin = y; m_edges[y].twin = x; }
        else if (lx)  { m_edges[x].twin = kInvalid; }
        else if (ly)  { m_edges[y].twin = kInvalid; }
    };
    rebond(bo, co);
    rebond(eo, fo);

    for (uint32_t eid : {a, b, c, d, e, f}) {
        if (eid < static_cast<uint32_t>(m_edges.size())) {
            const auto& edge = m_edges[eid];
            if (edge.src != kInvalid) {
                uint32_t dst = kInvalid;
                if (edge.next != kInvalid && edge.next < m_edges.size()) {
                    dst = m_edges[edge.next].src;
                }
                if (dst != kInvalid) {
                    uint64_t key = (static_cast<uint64_t>(edge.src) << 32) | static_cast<uint64_t>(dst);
                    m_edgeMap.erase(key);
                }
            }
        }
    }

    if (faceA < m_faces.size()) m_faces[faceA].edge = kInvalid;
    if (faceB < m_faces.size()) m_faces[faceB].edge = kInvalid;

    // Any vertex that referenced one of the six destroyed half-edges (vKeep and
    // the two wing apices vl/vr) must be re-rooted onto a surviving live edge.
    auto repairVertexEdge = [&](uint32_t v) {
        if (v >= m_verts.size()) return;
        uint32_t ve = m_verts[v].edge;
        const bool dead = (ve == kInvalid) || (ve >= m_edges.size()) ||
                          (m_edges[ve].face == kInvalid) || (m_edges[ve].src != v);
        if (!dead) return;
        m_verts[v].edge = kInvalid;
        for (uint32_t ei = 0; ei < static_cast<uint32_t>(m_edges.size()); ++ei) {
            if (m_edges[ei].src == v && m_edges[ei].face != kInvalid && m_edges[ei].next != kInvalid) {
                m_verts[v].edge = ei;
                break;
            }
        }
    };
    repairVertexEdge(vKeep);
    repairVertexEdge(vl);
    repairVertexEdge(vr);

    m_verts[vRemove].edge = kInvalid;

    return true;
}

// --- updateEdgeMap -----------------------------------------------------------

void HalfEdgeMesh::updateEdgeMap(uint32_t ei) {
    const auto& e = m_edges[ei];
    if (e.src == kInvalid || e.src >= m_verts.size()) return;

    uint32_t dst = kInvalid;
    if (e.next != kInvalid && e.next < m_edges.size()) {
        dst = m_edges[e.next].src;
    } else if (e.twin != kInvalid && e.twin < m_edges.size()) {
        dst = m_edges[e.twin].src;
    }

    if (dst == kInvalid) return;

    uint64_t key = (static_cast<uint64_t>(e.src) << 32) | static_cast<uint64_t>(dst);
    m_edgeMap[key] = ei;
}

// --- addEdgePair -------------------------------------------------------------

uint32_t HalfEdgeMesh::addEdgePair(uint32_t src, uint32_t dst, uint32_t face) {
    uint32_t idx0 = static_cast<uint32_t>(m_edges.size());
    HEEdge e0;
    e0.src = src;
    e0.face = face;
    m_edges.push_back(e0);

    uint32_t idx1 = static_cast<uint32_t>(m_edges.size());
    HEEdge e1;
    e1.src = dst;
    e1.face = face;
    m_edges.push_back(e1);

    m_edges[idx0].twin = idx1;
    m_edges[idx1].twin = idx0;

    updateEdgeMap(idx0);
    updateEdgeMap(idx1);

    return idx0;
}

// --- bevelVertex -------------------------------------------------------------
//  Bevels a vertex by offsetting its position along face normals.

bool HalfEdgeMesh::bevelVertex(uint32_t vertex, float distance) {
    if (vertex >= m_verts.size()) return false;
    if (distance <= 0.0f) return false;

    uint32_t start = m_verts[vertex].edge;
    if (start == kInvalid || start >= m_edges.size()) return false;

    auto incidentFaces = vertexFaces(vertex);
    Vec3 avgNormal{};
    for (uint32_t fi : incidentFaces) {
        uint32_t fe = m_faces[fi].edge;
        if (fe == kInvalid || fe >= m_edges.size()) continue;
        uint32_t eB = m_edges[fe].next;
        if (eB == kInvalid || eB >= m_edges.size()) continue;
        Vec3 a = m_positions[m_edges[fe].src];
        Vec3 b = m_positions[m_edges[eB].src];
        Vec3 c = m_positions[m_edges[m_edges[eB].next].src];
        Vec3 n = (b - a).cross(c - a);
        float nl = n.length();
        if (nl > 1e-10f) avgNormal = Vec3{avgNormal.x + n.x/nl, avgNormal.y + n.y/nl, avgNormal.z + n.z/nl};
    }
    float anLen = std::sqrt(avgNormal.x*avgNormal.x + avgNormal.y*avgNormal.y + avgNormal.z*avgNormal.z);
    if (anLen < 1e-10f) return false;
    m_positions[vertex].x += avgNormal.x / anLen * distance;
    m_positions[vertex].y += avgNormal.y / anLen * distance;
    m_positions[vertex].z += avgNormal.z / anLen * distance;
    return true;
}

// --- pokeFace ----------------------------------------------------------------
//  Creates a center vertex and fans the face into triangles.

bool HalfEdgeMesh::pokeFace(uint32_t faceIndex) {
    if (faceIndex >= m_faces.size()) return false;
    uint32_t start = m_faces[faceIndex].edge;
    if (start == kInvalid || start >= m_edges.size()) return false;

    // Walk the face collecting its perimeter half-edges (in order) and vertices.
    std::vector<uint32_t> perim;
    std::vector<uint32_t> faceVerts;
    Vec3 center{};
    uint32_t e = start;
    uint32_t safety = 0;
    do {
        uint32_t v = m_edges[e].src;
        perim.push_back(e);
        faceVerts.push_back(v);
        center.x += m_positions[v].x;
        center.y += m_positions[v].y;
        center.z += m_positions[v].z;
        uint32_t nxt = m_edges[e].next;
        if (nxt == kInvalid || nxt >= m_edges.size()) break;
        e = nxt;
        if (++safety > 256) break;
    } while (e != start);

    const size_t n = faceVerts.size();
    if (n < 3) return false;
    float inv = 1.0f / static_cast<float>(n);
    center = Vec3{center.x * inv, center.y * inv, center.z * inv};

    uint32_t centerV = static_cast<uint32_t>(m_verts.size());
    m_verts.push_back({kInvalid});
    m_positions.push_back(center);
    if (hasUVs()) {
        Vec2 uvCenter{};
        for (uint32_t v : faceVerts) { uvCenter.u += m_uvs[v].u; uvCenter.v += m_uvs[v].v; }
        m_uvs.push_back({uvCenter.u * inv, uvCenter.v * inv});
    }
    if (hasNormals()) {
        Vec3 nCenter{};
        for (uint32_t v : faceVerts) { nCenter.x+=m_normals[v].x; nCenter.y+=m_normals[v].y; nCenter.z+=m_normals[v].z; }
        m_normals.push_back({nCenter.x*inv, nCenter.y*inv, nCenter.z*inv});
    }

    // Create 2n spoke half-edges: out[i] = v_i -> center, in[i] = center -> v_i,
    // twinned to each other. Faces/next/prev are assigned when the fan is built.
    std::vector<uint32_t> outE(n), inE(n);
    for (size_t i = 0; i < n; ++i) {
        outE[i] = static_cast<uint32_t>(m_edges.size());
        HEEdge oe; oe.src = faceVerts[i]; m_edges.push_back(oe);
        inE[i] = static_cast<uint32_t>(m_edges.size());
        HEEdge ie; ie.src = centerV; m_edges.push_back(ie);
        m_edges[outE[i]].twin = inE[i];
        m_edges[inE[i]].twin = outE[i];
    }

    // Fan triangles: triangle i REUSES the perimeter half-edge perim[i]
    // (v_i -> v_{i+1}) as its base, plus out[i+1] (v_{i+1} -> center) and
    // in[i] (center -> v_i). Adjacent triangles share a spoke vertex, so
    // out[j]/in[j] are naturally twins. Reuse the old face slot for triangle 0
    // (so no face is orphaned) and allocate fresh slots for the rest.
    for (size_t i = 0; i < n; ++i) {
        uint32_t f = (i == 0) ? faceIndex : static_cast<uint32_t>(m_faces.size());
        if (i != 0) m_faces.push_back({});
        uint32_t p = perim[i];
        uint32_t o = outE[(i + 1) % n];
        uint32_t in = inE[i];
        m_edges[p].face = f;  m_edges[p].next = o;   m_edges[p].prev = in;
        m_edges[o].face = f;  m_edges[o].next = in;  m_edges[o].prev = p;
        m_edges[in].face = f; m_edges[in].next = p;  m_edges[in].prev = o;
        m_faces[f].edge = p;
    }

    m_verts[centerV].edge = inE[0];
    for (size_t i = 0; i < n; ++i) {
        // perim[i] is still live and rooted at v_i; keep the vertex ptr valid.
        m_verts[faceVerts[i]].edge = perim[i];
        updateEdgeMap(perim[i]);
        updateEdgeMap(outE[i]);
        updateEdgeMap(inE[i]);
    }
    return true;
}

// --- connectVertices ---------------------------------------------------------
//  Connects two vertices sharing a face with a new diagonal edge.

bool HalfEdgeMesh::connectVertices(uint32_t v0, uint32_t v1) {
    if (v0 == v1 || v0 >= m_verts.size() || v1 >= m_verts.size()) return false;
    if (findEdge(v0, v1) != kInvalid || findEdge(v1, v0) != kInvalid) return false;

    uint32_t commonFace = kInvalid;
    auto facesA = vertexFaces(v0);
    for (uint32_t fa : facesA) {
        auto facesB = vertexFaces(v1);
        for (uint32_t fb : facesB) {
            if (fa == fb) { commonFace = fa; break; }
        }
        if (commonFace != kInvalid) break;
    }
    if (commonFace == kInvalid || commonFace >= m_faces.size()) return false;

    // Walk face to collect the edge chain and vertex order
    uint32_t start = m_faces[commonFace].edge;
    if (start == kInvalid || start >= m_edges.size()) return false;

    std::vector<uint32_t> chainEdges;
    std::vector<uint32_t> chainVerts;
    uint32_t walk = start;
    uint32_t safety = 0;
    do {
        chainEdges.push_back(walk);
        chainVerts.push_back(m_edges[walk].src);
        uint32_t nxt = m_edges[walk].next;
        if (nxt == kInvalid || nxt >= m_edges.size()) break;
        walk = nxt;
        if (++safety > 256) break;
    } while (walk != start);

    if (chainVerts.size() < 3) return false;

    // Find positions of v0 and v1 in the chain
    int pos0 = -1, pos1 = -1;
    for (size_t i = 0; i < chainVerts.size(); ++i) {
        if (chainVerts[i] == v0) pos0 = static_cast<int>(i);
        if (chainVerts[i] == v1) pos1 = static_cast<int>(i);
    }
    if (pos0 < 0 || pos1 < 0) return false;

    // Ensure pos0 < pos1 (swap if needed)
    if (pos0 > pos1) { std::swap(pos0, pos1); }

    // Reject adjacent vertices (they're already connected by an edge)
    int nVerts = static_cast<int>(chainVerts.size());
    if (pos1 - pos0 == 1 || (pos0 == 0 && pos1 == nVerts - 1)) return false;

    // Kill old face
    m_faces[commonFace].edge = kInvalid;

    // Create two new faces
    uint32_t faceA = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});
    uint32_t faceB = static_cast<uint32_t>(m_faces.size());
    m_faces.push_back({});

    // Chain A: edges from pos0 to pos1-1 (v0 -> ... -> edge before v1)
    // The last edge in chain A leads to v1
    for (int i = pos0; i < pos1; ++i) {
        m_edges[chainEdges[i]].face = faceA;
    }
    m_faces[faceA].edge = chainEdges[pos0];

    // Chain B: edges from pos1 to the end, then 0 to pos0-1 (v1 -> ... -> v0)
    for (int i = pos1; i < nVerts; ++i) {
        m_edges[chainEdges[i]].face = faceB;
    }
    for (int i = 0; i < pos0; ++i) {
        m_edges[chainEdges[i]].face = faceB;
    }
    m_faces[faceB].edge = chainEdges[pos1 % nVerts];

    // After the pos0<pos1 ordering, the endpoints are the chain vertices AT
    // those positions — NOT necessarily the original (v0,v1), which the pos
    // swap may have decoupled. Segment A runs vA -> ... -> vB; close it with the
    // cross edge vB -> vA (face A). Segment B runs vB -> ... -> vA; close it
    // with vA -> vB (face B).
    const uint32_t vA = chainVerts[pos0];  // start of segment A
    const uint32_t vB = chainVerts[pos1];  // end of segment A / start of segment B

    uint32_t eCrossA = addEdgePair(vB, vA, faceA);  // vB -> vA, closes face A
    uint32_t eCrossB = m_edges[eCrossA].twin;        // vA -> vB, on face B
    m_edges[eCrossB].face = faceB;

    // Close each sub-face cycle with the diagonal, using the known chain layout.
    // (The previous code linked the cross edges to the *original* successors,
    //  which lived on the other sub-face — hence "face-cycle spans multiple
    //  faces". Wire them to each sub-face's own first edge instead.)
    const uint32_t firstA = chainEdges[pos0];        // starts at vA
    const uint32_t lastA  = chainEdges[pos1 - 1];    // ends at vB
    const uint32_t firstB = chainEdges[pos1];        // starts at vB
    const uint32_t lastB  = chainEdges[(pos0 == 0) ? (nVerts - 1) : (pos0 - 1)]; // ends at vA

    m_edges[lastA].next = eCrossA; m_edges[eCrossA].prev = lastA;
    m_edges[eCrossA].next = firstA; m_edges[firstA].prev = eCrossA;

    m_edges[lastB].next = eCrossB; m_edges[eCrossB].prev = lastB;
    m_edges[eCrossB].next = firstB; m_edges[firstB].prev = eCrossB;

    // Keep the diagonal endpoints rooted on live outgoing edges.
    m_verts[vA].edge = firstA;  // vA -> ...
    m_verts[vB].edge = firstB;  // vB -> ...

    updateEdgeMap(eCrossA);
    updateEdgeMap(eCrossB);

    return true;
}

// --- gridFill ----------------------------------------------------------------
//  Fills a closed boundary loop with quad/triangle fans.
//  For even loop sizes, pairs opposite vertices. For odd, uses center poke.

bool HalfEdgeMesh::gridFill(const std::vector<uint32_t>& boundaryLoop) {
    if (boundaryLoop.size() < 3) return false;

    for (uint32_t v : boundaryLoop) {
        if (v >= m_verts.size()) return false;
    }

    // Verify all boundary vertices have at least one boundary edge
    for (size_t i = 0; i < boundaryLoop.size(); ++i) {
        uint32_t v0 = boundaryLoop[i];
        uint32_t v1 = boundaryLoop[(i + 1) % boundaryLoop.size()];
        uint32_t he = findEdge(v0, v1);
        if (he == kInvalid || m_edges[he].twin != kInvalid) continue;
    }

    uint32_t n = static_cast<uint32_t>(boundaryLoop.size());

    if (n % 2 == 0) {
        // Quad fill: pair vertices across the loop
        uint32_t half = n / 2;
        for (uint32_t i = 0; i < half - 1; ++i) {
            uint32_t vA0 = boundaryLoop[i];
            uint32_t vA1 = boundaryLoop[i + 1];
            uint32_t vB0 = boundaryLoop[n - 1 - i];
            uint32_t vB1 = boundaryLoop[n - 2 - i];

            uint32_t newFace = static_cast<uint32_t>(m_faces.size());
            m_faces.push_back({});

            if (findEdge(vA0, vA1) != kInvalid && findEdge(vB1, vB0) != kInvalid) {
                uint32_t e0 = addEdgePair(vA0, vA1, newFace);
                uint32_t e1 = addEdgePair(vA1, vB0, newFace);
                uint32_t e2 = addEdgePair(vB0, vB1, newFace);
                uint32_t e3 = addEdgePair(vB1, vA0, newFace);

                m_edges[e0].next = e1; m_edges[e1].next = e2;
                m_edges[e2].next = e3; m_edges[e3].next = e0;
                m_edges[e0].prev = e3; m_edges[e1].prev = e0;
                m_edges[e2].prev = e1; m_edges[e3].prev = e2;

                m_faces[newFace].edge = e0;
            }
        }
    } else {
        // Odd: use center vertex fan
        Vec3 center{};
        for (uint32_t v : boundaryLoop) {
            center.x += m_positions[v].x;
            center.y += m_positions[v].y;
            center.z += m_positions[v].z;
        }
        float inv = 1.0f / static_cast<float>(n);
        center = Vec3{center.x * inv, center.y * inv, center.z * inv};

        uint32_t centerV = static_cast<uint32_t>(m_verts.size());
        m_verts.push_back({kInvalid});
        m_positions.push_back(center);
        if (hasUVs()) m_uvs.push_back({});
        if (hasNormals()) m_normals.push_back({});

        for (uint32_t i = 0; i < n; ++i) {
            uint32_t v0 = boundaryLoop[i];
            uint32_t v1 = boundaryLoop[(i + 1) % n];

            uint32_t newFace = static_cast<uint32_t>(m_faces.size());
            m_faces.push_back({});

            uint32_t e0 = addEdgePair(v0, centerV, newFace);
            uint32_t e1 = addEdgePair(centerV, v1, newFace);
            uint32_t e2 = addEdgePair(v1, v0, newFace);

            m_edges[e0].next = e1; m_edges[e1].next = e2; m_edges[e2].next = e0;
            m_edges[e0].prev = e2; m_edges[e1].prev = e0; m_edges[e2].prev = e1;

            m_faces[newFace].edge = e0;
        }
    }

    return true;
}

// --- bevelEdgesHEM -----------------------------------------------------------
//  Bevels selected edges with topology-aware vertex mitering.
//  Creates new vertices per miter point, bevel strip faces, and remaps originals.

bool HalfEdgeMesh::bevelEdgesHEM(const std::vector<uint32_t>& edges, float distance, uint32_t segments) {
    if (edges.empty() || distance <= 0.0f) return false;
    segments = std::max(segments, 1u);

    // Build sets of involved edges and vertices
    std::unordered_set<uint32_t> edgeSet(edges.begin(), edges.end());
    std::unordered_set<uint32_t> vertSet;
    for (uint32_t ei : edges) {
        if (ei >= m_edges.size()) continue;
        vertSet.insert(m_edges[ei].src);
        uint32_t t = m_edges[ei].twin;
        if (t != kInvalid && t < m_edges.size()) vertSet.insert(m_edges[t].src);
    }

    // Phase 1: For each vertex, compute offset direction from incident beveled edges
    struct MiterData { Vec3 offset; bool valid = false; };
    std::unordered_map<uint32_t, MiterData> miters;

    for (uint32_t v : vertSet) {
        if (v >= m_verts.size()) continue;
        Vec3 sumDir{};
        uint32_t count = 0;

        auto neighbors = vertexNeighbors(v);
        for (uint32_t nb : neighbors) {
            uint32_t he = findEdge(v, nb);
            if (he == kInvalid) continue;
            if (edgeSet.count(he)) {
                sumDir.x += m_positions[nb].x - m_positions[v].x;
                sumDir.y += m_positions[nb].y - m_positions[v].y;
                sumDir.z += m_positions[nb].z - m_positions[v].z;
                ++count;
            } else if (m_edges[he].twin < m_edges.size() && edgeSet.count(m_edges[he].twin)) {
                sumDir.x += m_positions[nb].x - m_positions[v].x;
                sumDir.y += m_positions[nb].y - m_positions[v].y;
                sumDir.z += m_positions[nb].z - m_positions[v].z;
                ++count;
            }
        }
        if (count == 0) continue;

        float invC = 1.0f / static_cast<float>(count);
        sumDir = Vec3{sumDir.x * invC, sumDir.y * invC, sumDir.z * invC};

        // Compute miter: perpendicular to average edge direction
        Vec3 miter{sumDir.z, sumDir.y, -sumDir.x};
        float ml = std::sqrt(miter.x*miter.x + miter.y*miter.y + miter.z*miter.z);
        if (ml < 1e-10f) miter = {1, 0, 0};
        else miter = Vec3{miter.x/ml, miter.y/ml, miter.z/ml};

        miters[v] = {miter, true};
    }

    // Phase 2: Create offset vertices for miter points
    std::unordered_map<uint32_t, uint32_t> miterToVert;
    for (auto& [v, md] : miters) {
        if (!md.valid) continue;
        Vec3 newPos{
            m_positions[v].x + md.offset.x * distance,
            m_positions[v].y + md.offset.y * distance,
            m_positions[v].z + md.offset.z * distance,
        };
        uint32_t nv = static_cast<uint32_t>(m_verts.size());
        m_verts.push_back({kInvalid});
        m_positions.push_back(newPos);
        if (hasUVs()) m_uvs.push_back(m_uvs[v]);
        if (hasNormals()) m_normals.push_back(m_normals[v]);
        miterToVert[v] = nv;
    }

    // Phase 3: For each beveled edge, create bevel quad faces
    for (uint32_t ei : edges) {
        if (ei >= m_edges.size()) continue;
        uint32_t t = m_edges[ei].twin;
        if (t == kInvalid || t >= m_edges.size()) continue;

        uint32_t src = m_edges[ei].src;
        uint32_t dst = m_edges[t].src;

        auto itSrc = miterToVert.find(src);
        auto itDst = miterToVert.find(dst);
        if (itSrc == miterToVert.end() || itDst == miterToVert.end()) continue;

        uint32_t newSrc = itSrc->second;
        uint32_t newDst = itDst->second;

        // Create bevel strip face (quad: src -> newSrc -> newDst -> dst)
        uint32_t bf = static_cast<uint32_t>(m_faces.size());
        m_faces.push_back({});

        uint32_t e0 = addEdgePair(src, newSrc, bf);
        uint32_t e1 = addEdgePair(newSrc, newDst, bf);
        uint32_t e2 = addEdgePair(newDst, dst, bf);
        uint32_t e3 = addEdgePair(dst, src, bf);

        m_edges[e0].next = e1; m_edges[e1].next = e2;
        m_edges[e2].next = e3; m_edges[e3].next = e0;
        m_edges[e0].prev = e3; m_edges[e1].prev = e0;
        m_edges[e2].prev = e1; m_edges[e3].prev = e2;

        m_faces[bf].edge = e0;
    }

    // Phase 4: Remap original face edges to use new miter vertices
    for (uint32_t fi = 0; fi < m_faces.size(); ++fi) {
        uint32_t fe = m_faces[fi].edge;
        if (fe == kInvalid || fe >= m_edges.size()) continue;

        // Check if any edge in this face uses a beveled vertex
        uint32_t walk = fe;
        uint32_t safety = 0;
        do {
            uint32_t v = m_edges[walk].src;
            auto it = miterToVert.find(v);
            if (it != miterToVert.end()) {
                m_edges[walk].src = it->second;
                updateEdgeMap(walk);
            }
            uint32_t nxt = m_edges[walk].next;
            if (nxt == kInvalid || nxt >= m_edges.size()) break;
            walk = nxt;
            if (++safety > 256) break;
        } while (walk != fe);
    }

    (void)segments;
    return true;
}

// --- extrudeFaces ------------------------------------------------------------
//  Extrudes selected faces with edge-stitching.
//  Shared edges between selected faces share offset vertices (no duplicate walls).
//  Boundary edges between selected and unselected faces create wall quads.

bool HalfEdgeMesh::extrudeFaces(const std::vector<uint32_t>& faceIndices, float distance, bool keepOriginal) {
    if (faceIndices.empty() || std::abs(distance) < 1e-10f) return false;

    std::unordered_set<uint32_t> selFaces(faceIndices.begin(), faceIndices.end());

    // Validate faces
    for (uint32_t fi : faceIndices) {
        if (fi >= m_faces.size() || m_faces[fi].edge == kInvalid) return false;
    }

    // Phase 1: Find boundary edges (edges between selected and unselected faces)
    struct BoundaryEdge {
        uint32_t he;     // half-edge in the selected face (going to boundary)
        uint32_t twin;   // twin half-edge (in unselected face or boundary)
    };
    std::vector<BoundaryEdge> boundaryEdges;

    // Collect all edges incident to selected faces
    std::unordered_set<uint32_t> visitedEdges;
    for (uint32_t fi : faceIndices) {
        uint32_t start = m_faces[fi].edge;
        uint32_t e = start;
        uint32_t safety = 0;
        do {
            if (visitedEdges.count(e)) break;
            visitedEdges.insert(e);
            uint32_t t = m_edges[e].twin;
            uint32_t otherFace = (t < m_edges.size()) ? m_edges[t].face : kInvalid;
            if (otherFace == kInvalid || !selFaces.count(otherFace)) {
                boundaryEdges.push_back({e, t});
            }
            uint32_t nxt = m_edges[e].next;
            if (nxt == kInvalid || nxt >= m_edges.size()) break;
            e = nxt;
            if (++safety > 256) break;
        } while (e != start);
    }

    // Phase 2: Compute offset direction per vertex
    // For each vertex used by selected faces, average face normals
    std::unordered_map<uint32_t, Vec3> vertexOffsets;
    std::unordered_set<uint32_t> selectedVerts;

    auto faceNormal = [&](uint32_t fi) -> Vec3 {
        uint32_t fe = m_faces[fi].edge;
        if (fe == kInvalid || fe >= m_edges.size()) return {0, 1, 0};
        Vec3 a = m_positions[m_edges[fe].src];
        Vec3 b = m_positions[m_edges[m_edges[fe].next].src];
        Vec3 c = m_positions[m_edges[m_edges[m_edges[fe].next].next].src];
        Vec3 n = (b - a).cross(c - a);
        float len = n.length();
        return (len > 1e-10f) ? Vec3{n.x/len, n.y/len, n.z/len} : Vec3{0, 1, 0};
    };

    for (uint32_t fi : faceIndices) {
        uint32_t start = m_faces[fi].edge;
        uint32_t e = start;
        uint32_t safety = 0;
        Vec3 fn = faceNormal(fi);
        do {
            uint32_t v = m_edges[e].src;
            selectedVerts.insert(v);
            auto& off = vertexOffsets[v];
            off = Vec3{off.x + fn.x, off.y + fn.y, off.z + fn.z};
            uint32_t nxt = m_edges[e].next;
            if (nxt == kInvalid || nxt >= m_edges.size()) break;
            e = nxt;
            if (++safety > 256) break;
        } while (e != start);
    }

    // Normalize vertex offsets
    for (auto& [v, off] : vertexOffsets) {
        float len = std::sqrt(off.x*off.x + off.y*off.y + off.z*off.z);
        if (len > 1e-10f) off = Vec3{off.x/len, off.y/len, off.z/len};
        else off = Vec3{0, 1, 0};
    }

    // Phase 3: Create offset vertices
    std::unordered_map<uint32_t, uint32_t> oldToNew;
    for (uint32_t v : selectedVerts) {
        const auto& off = vertexOffsets[v];
        Vec3 np{
            m_positions[v].x + off.x * distance,
            m_positions[v].y + off.y * distance,
            m_positions[v].z + off.z * distance,
        };
        uint32_t nv = static_cast<uint32_t>(m_verts.size());
        m_verts.push_back({kInvalid});
        m_positions.push_back(np);
        if (hasUVs()) m_uvs.push_back(m_uvs[v]);
        if (hasNormals()) m_normals.push_back(off);
        oldToNew[v] = nv;
    }

    // Phase 4: Create a wall quad along each boundary edge.
    //
    // For boundary edge he (src -> dst) with old external twin be.twin (dst ->
    // src in the neighbour / boundary), the wall cycle is
    //     wb: src -> dst      (bottom, re-twinned to be.twin)
    //     wr: dst -> newDst    (right vertical)
    //     wt: newDst -> newSrc (top, re-twinned to the cap edge he)
    //     wl: newSrc -> src    (left vertical)
    // Verticals are shared between adjacent walls (wr of one == reverse of wl of
    // the next around a shared rim vertex) and are paired via a local reverse
    // map. addEdgePair() is deliberately NOT used here: it would twin the two
    // directions to each other on the same face, which is wrong for walls.
    std::unordered_map<uint64_t, uint32_t> wallEdgeMap;
    auto wkey = [](uint32_t a, uint32_t b) {
        return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
    };
    auto makeWallHE = [&](uint32_t s, uint32_t d, uint32_t face) -> uint32_t {
        uint32_t idx = static_cast<uint32_t>(m_edges.size());
        HEEdge he2; he2.src = s; he2.face = face; m_edges.push_back(he2);
        auto it = wallEdgeMap.find(wkey(d, s));
        if (it != wallEdgeMap.end()) {
            m_edges[idx].twin = it->second;
            m_edges[it->second].twin = idx;
        }
        wallEdgeMap[wkey(s, d)] = idx;
        return idx;
    };

    for (const auto& be : boundaryEdges) {
        uint32_t he = be.he;
        if (he >= m_edges.size()) continue;

        uint32_t src = m_edges[he].src;
        uint32_t nxt = m_edges[he].next;
        if (nxt >= m_edges.size()) continue;
        uint32_t dst = m_edges[nxt].src;

        auto itSrc = oldToNew.find(src);
        auto itDst = oldToNew.find(dst);
        if (itSrc == oldToNew.end() || itDst == oldToNew.end()) continue;

        uint32_t newSrc = itSrc->second;
        uint32_t newDst = itDst->second;

        uint32_t wallFace = static_cast<uint32_t>(m_faces.size());
        m_faces.push_back({});

        uint32_t wb = makeWallHE(src, dst, wallFace);
        uint32_t wr = makeWallHE(dst, newDst, wallFace);
        uint32_t wt = makeWallHE(newDst, newSrc, wallFace);
        uint32_t wl = makeWallHE(newSrc, src, wallFace);

        m_edges[wb].next = wr; m_edges[wr].next = wt;
        m_edges[wt].next = wl; m_edges[wl].next = wb;
        m_edges[wb].prev = wl; m_edges[wr].prev = wb;
        m_edges[wt].prev = wr; m_edges[wl].prev = wt;
        m_faces[wallFace].edge = wb;

        // wb re-twins the old neighbour edge (open boundary => stays boundary).
        if (be.twin != kInvalid && be.twin < m_edges.size()) {
            m_edges[wb].twin = be.twin;
            m_edges[be.twin].twin = wb;
        }
        // wt re-twins the cap edge he (Phase 5 remaps it to newSrc -> newDst).
        m_edges[wt].twin = he;
        m_edges[he].twin = wt;

        // Rim/new vertices are rooted on live wall edges.
        m_verts[src].edge = wb;
        m_verts[dst].edge = wr;
        m_verts[newDst].edge = wt;
        m_verts[newSrc].edge = wl;

        updateEdgeMap(wb); updateEdgeMap(wr); updateEdgeMap(wt); updateEdgeMap(wl);
    }

    // Phase 5: Remap selected faces to use offset vertices
    for (uint32_t fi : faceIndices) {
        uint32_t start = m_faces[fi].edge;
        uint32_t e = start;
        uint32_t safety = 0;
        do {
            uint32_t v = m_edges[e].src;
            auto it = oldToNew.find(v);
            if (it != oldToNew.end()) {
                m_edges[e].src = it->second;
                updateEdgeMap(e);
            }
            uint32_t nxt = m_edges[e].next;
            if (nxt == kInvalid || nxt >= m_edges.size()) break;
            e = nxt;
            if (++safety > 256) break;
        } while (e != start);
    }

    // Phase 6: Keep or remove the original (now offset) cap faces.
    // keepOriginal=false discards the cap, leaving an open-topped wall skirt.
    // The cap half-edges must be tombstoned too (not just the face), and their
    // wall-top twins (wt) re-opened to boundary — otherwise those walls point at
    // a dead face / dead twin.
    if (!keepOriginal) {
        for (uint32_t fi : faceIndices) {
            if (fi >= m_faces.size() || m_faces[fi].edge == kInvalid) continue;
            uint32_t start = m_faces[fi].edge;
            uint32_t e = start;
            uint32_t safety = 0;
            do {
                uint32_t nxt = m_edges[e].next;
                uint32_t tw = m_edges[e].twin;
                if (tw != kInvalid && tw < m_edges.size()) m_edges[tw].twin = kInvalid;
                m_edges[e].face = kInvalid;
                m_edges[e].twin = kInvalid;
                if (nxt == kInvalid || nxt >= m_edges.size()) break;
                e = nxt;
                if (++safety > 256) break;
            } while (e != start);
            m_faces[fi].edge = kInvalid;
        }
    }

    // Re-root any old selected vertex whose edge pointer went stale after the
    // remap / cap removal. A vertex fully absorbed by its offset copy (interior
    // of a multi-face selection) becomes orphaned (kInvalid); rim vertices point
    // at their wall edges.
    for (uint32_t v : selectedVerts) {
        uint32_t cur = m_verts[v].edge;
        const bool ok = cur != kInvalid && cur < m_edges.size() &&
                        m_edges[cur].face != kInvalid && m_edges[cur].src == v;
        if (ok) continue;
        m_verts[v].edge = kInvalid;
        for (uint32_t e = 0; e < static_cast<uint32_t>(m_edges.size()); ++e) {
            if (m_edges[e].face != kInvalid && m_edges[e].src == v) {
                m_verts[v].edge = e;
                break;
            }
        }
    }

    return true;
}

} // namespace nexus::geometry
