#include <nexus/geometry/MeshVertexMerge.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/Tolerance.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

// Rebuild `mesh` after collapsing vertices per `remap` (remap[i] = the surviving
// representative index of vertex i). Faces are remapped and any that collapse to fewer than
// three distinct vertices are dropped; surviving vertices are compacted. Returns the number
// of vertices removed, or 0 (leaving `mesh` unchanged) if the rebuilt topology is invalid.
//
// This goes through the public toMesh/fromMesh boundary on purpose: HalfEdgeMesh exposes no
// mutable half-edge accessor, and rebuilding is the only integrity-safe way to weld — the
// original code tried to poke edge().src directly and never even compiled.
uint32_t rebuildWithRemap(HalfEdgeMesh& mesh, const std::vector<uint32_t>& remap) {
    const Mesh src = mesh.toMesh(false);  // preserve n-gon faces
    const auto& srcPos = src.attributes().positions();
    if (remap.size() != srcPos.size()) return 0;

    std::vector<uint32_t> compact(srcPos.size(), UINT32_MAX);
    std::vector<Vec3> newPos;
    auto survivor = [&](uint32_t v) -> uint32_t {
        const uint32_t r = remap[v];
        if (compact[r] == UINT32_MAX) {
            compact[r] = static_cast<uint32_t>(newPos.size());
            newPos.push_back(srcPos[r]);
        }
        return compact[r];
    };

    Mesh out;
    const auto& topo = src.topology();
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& f = topo.face(fi);
        std::vector<uint32_t> idx;
        idx.reserve(f.indices.size());
        for (uint32_t v : f.indices) {
            if (v >= srcPos.size()) { idx.clear(); break; }
            const uint32_t s = survivor(v);
            if (idx.empty() || idx.back() != s) idx.push_back(s);  // drop consecutive dupes
        }
        if (idx.size() >= 2 && idx.front() == idx.back()) idx.pop_back();  // close-loop dup
        if (idx.size() >= 3) {
            Face nf;
            nf.indices = std::move(idx);
            out.topology().addFace(std::move(nf));
        }
    }
    out.attributes().setPositions(std::vector<Vec3>(newPos));

    auto rebuilt = HalfEdgeMesh::fromMesh(out);
    if (!rebuilt) return 0;

    const uint32_t removed = static_cast<uint32_t>(srcPos.size() - newPos.size());
    mesh = std::move(*rebuilt);
    return removed;
}

} // namespace

MergeReport MeshVertexMerge::mergeToVertex(HalfEdgeMesh& mesh, uint32_t sourceIdx, uint32_t targetIdx) {
    const uint32_t V = static_cast<uint32_t>(mesh.positions().size());
    if (sourceIdx == targetIdx) return {false, 0, 0};
    if (sourceIdx >= V || targetIdx >= V) return {false, 0, 0};

    std::vector<uint32_t> remap(V);
    for (uint32_t i = 0; i < V; ++i) remap[i] = i;
    remap[sourceIdx] = targetIdx;

    const uint32_t removed = rebuildWithRemap(mesh, remap);
    MergeReport report;
    report.success = removed > 0;
    report.verticesRemoved = removed;
    report.edgesCollapsed = 0;
    return report;
}

MergeReport MeshVertexMerge::mergeByDistance(HalfEdgeMesh& mesh, float tolerance) {
    const auto& positions = mesh.positions();
    const uint32_t V = static_cast<uint32_t>(positions.size());

    // Absolute-distance coincidence via the central Tolerance module (squared distance, no
    // sqrt). Each coincident vertex maps to the lowest-indexed member of its group, so the
    // remap is flat (no chaining) and applied in a single rebuild.
    const Tolerance tol{tolerance, 0.f};
    std::vector<uint32_t> remap(V);
    for (uint32_t i = 0; i < V; ++i) remap[i] = i;

    uint32_t pending = 0;
    for (uint32_t i = 0; i < V; ++i) {
        if (remap[i] != i) continue;  // already merged into an earlier group
        for (uint32_t j = i + 1; j < V; ++j) {
            if (remap[j] == j && coincident(positions[i], positions[j], tol)) {
                remap[j] = i;
                ++pending;
            }
        }
    }

    MergeReport report;
    if (pending == 0) { report.success = false; return report; }

    const uint32_t removed = rebuildWithRemap(mesh, remap);
    report.success = removed > 0;
    report.verticesRemoved = removed;
    report.edgesCollapsed = 0;
    return report;
}

} // namespace nexus::geometry
