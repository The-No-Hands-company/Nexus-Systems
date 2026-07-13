// Foundation: the half-edge Euler operators (flip / split / collapse) must
// preserve the raw connectivity invariants. checkIntegrity() is the
// authoritative post-condition; these tests prove every *successful* operator
// application leaves the live sub-complex integrity-clean, and that the closed
// genus-0 Euler characteristic is preserved where the operator is χ-neutral.

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

namespace {
// A closed, watertight, triangulated genus-0 solid.
Mesh triBox()
{
    Mesh m = makeBox(1.f, 1.f, 1.f);
    (void)m.topology().triangulate();
    return m;
}

HalfEdgeMesh triBoxHE()
{
    auto hem = HalfEdgeMesh::fromMesh(triBox());
    EXPECT_TRUE(hem.has_value());
    return std::move(*hem);
}

// Euler characteristic from live counts only (orphan positions left by
// tombstoning collapses don't participate). Valid for a *closed* mesh, where
// every live half-edge is paired, so undirected edges = liveEdges / 2.
uint32_t closedEuler(const HalfEdgeMesh::IntegrityReport& r)
{
    return r.liveVerts - r.liveEdges / 2 + r.liveFaces;
}
}  // namespace

// A freshly-built half-edge mesh from a valid solid is integrity-clean.
TEST(HEMEulerIntegrity, FreshCubeIsIntegrityClean)
{
    const HalfEdgeMesh hem = triBoxHE();
    const auto r = hem.checkIntegrity();
    EXPECT_TRUE(r.ok) << r.reason;
    EXPECT_EQ(r.boundaryEdges, 0u);          // closed solid — no boundary
    EXPECT_EQ(r.liveVerts, 8u);              // cube corners
    EXPECT_EQ(r.liveFaces, 12u);             // 6 quads → 12 triangles
    EXPECT_EQ(r.liveEdges, 36u);             // 3 half-edges × 12 tris
    // Euler χ = V - E + F = 8 - 18 + 12 = 2 (E = liveEdges/2, all interior).
    EXPECT_EQ(r.liveVerts - r.liveEdges / 2 + r.liveFaces, 2u);
}

// Every flip that the operator accepts must yield an integrity-clean mesh that
// is still a closed genus-0 solid (flip is χ-neutral: ΔV=0, ΔE=0, ΔF=0).
TEST(HEMEulerIntegrity, EverySuccessfulFlipPreservesIntegrity)
{
    const HalfEdgeMesh base = triBoxHE();
    int flips = 0;
    for (uint32_t e = 0; e < base.edgeCount(); ++e) {
        HalfEdgeMesh hem = base;  // fresh copy per attempt
        if (!hem.flipEdge(e)) continue;
        ++flips;
        const auto r = hem.checkIntegrity();
        ASSERT_TRUE(r.ok) << "flip edge " << e << ": " << r.reason;
        EXPECT_EQ(r.liveFaces, 12u) << "flip changed face count, edge " << e;
        EXPECT_EQ(r.boundaryEdges, 0u) << "flip opened a boundary, edge " << e;
        EXPECT_EQ(closedEuler(r), 2u) << "flip broke genus-0, edge " << e;
    }
    EXPECT_GT(flips, 0) << "no edge was flippable — test is vacuous";
}

// Every collapse the operator accepts must yield an integrity-clean mesh. A
// triangle-pair edge collapse on a closed solid is χ-neutral
// (ΔV=-1, ΔE=-3, ΔF=-2 ⇒ Δχ=0), so it stays a closed genus-0 shell.
TEST(HEMEulerIntegrity, EverySuccessfulCollapsePreservesIntegrity)
{
    const HalfEdgeMesh base = triBoxHE();
    int collapses = 0;
    for (uint32_t e = 0; e < base.edgeCount(); ++e) {
        HalfEdgeMesh hem = base;  // fresh copy per attempt
        const auto& he = base.edge(e);
        if (he.twin == HalfEdgeMesh::kInvalid) continue;
        // Collapse onto the surviving endpoint's position (dst of e).
        const uint32_t vKeep = base.edge(he.twin).src;
        if (vKeep >= base.positions().size()) continue;
        const nexus::render::Vec3 target = base.positions()[vKeep];
        if (!hem.collapseEdge(e, target)) continue;
        ++collapses;
        const auto r = hem.checkIntegrity();
        ASSERT_TRUE(r.ok) << "collapse edge " << e << ": " << r.reason;
        EXPECT_EQ(r.boundaryEdges, 0u) << "collapse opened a boundary, edge " << e;
        EXPECT_EQ(closedEuler(r), 2u) << "collapse broke genus-0, edge " << e;
    }
    EXPECT_GT(collapses, 0) << "no edge was collapsible — test is vacuous";
}

// Split is a subdivision: it adds a vertex and faces but must stay integrity-
// clean and genus-0.
TEST(HEMEulerIntegrity, EverySuccessfulSplitPreservesIntegrity)
{
    const HalfEdgeMesh base = triBoxHE();
    int splits = 0;
    for (uint32_t e = 0; e < base.edgeCount(); ++e) {
        HalfEdgeMesh hem = base;
        if (!hem.splitEdge(e)) continue;
        ++splits;
        const auto r = hem.checkIntegrity();
        ASSERT_TRUE(r.ok) << "split edge " << e << ": " << r.reason;
        EXPECT_EQ(r.boundaryEdges, 0u) << "split opened a boundary, edge " << e;
        EXPECT_EQ(closedEuler(r), 2u) << "split broke genus-0, edge " << e;
    }
    EXPECT_GT(splits, 0) << "no edge was splittable — test is vacuous";
}

namespace {
// Quad box (un-triangulated): 6 quad faces, exercises the n-gon paths.
HalfEdgeMesh quadBoxHE()
{
    auto hem = HalfEdgeMesh::fromMesh(makeBox(1.f, 1.f, 1.f));
    EXPECT_TRUE(hem.has_value());
    return std::move(*hem);
}
}  // namespace

// insertEdgeVertex subdivides an edge without splitting faces (turns the two
// incident quads into pentagons). Regression for two bugs the validator caught:
// crossed twin pairing and an un-repaired endpoint vertex pointer.
TEST(HEMEulerIntegrity, EverySuccessfulInsertEdgeVertexPreservesIntegrity)
{
    const HalfEdgeMesh base = quadBoxHE();
    int inserts = 0;
    for (uint32_t e = 0; e < base.edgeCount(); ++e) {
        HalfEdgeMesh hem = base;
        if (!hem.insertEdgeVertex(e, 0.5f)) continue;
        ++inserts;
        const auto r = hem.checkIntegrity();
        ASSERT_TRUE(r.ok) << "insertEdgeVertex edge " << e << ": " << r.reason;
        EXPECT_EQ(r.boundaryEdges, 0u) << "insertEdgeVertex opened a boundary, edge " << e;
        EXPECT_EQ(closedEuler(r), 2u) << "insertEdgeVertex broke genus-0, edge " << e;
    }
    EXPECT_GT(inserts, 0) << "no edge accepted insertEdgeVertex — test is vacuous";
}

// splitEdge on a quad face delegates to insertEdgeVertex; guard that path too.
TEST(HEMEulerIntegrity, SplitEdgeOnQuadPreservesIntegrity)
{
    const HalfEdgeMesh base = quadBoxHE();
    int splits = 0;
    for (uint32_t e = 0; e < base.edgeCount(); ++e) {
        HalfEdgeMesh hem = base;
        if (!hem.splitEdge(e)) continue;
        ++splits;
        const auto r = hem.checkIntegrity();
        ASSERT_TRUE(r.ok) << "splitEdge(quad) edge " << e << ": " << r.reason;
        EXPECT_EQ(r.boundaryEdges, 0u) << "splitEdge(quad) opened a boundary, edge " << e;
    }
    EXPECT_GT(splits, 0) << "no quad edge accepted splitEdge — test is vacuous";
}

// bevelVertex is (and must stay) integrity-clean.
TEST(HEMEulerIntegrity, BevelVertexPreservesIntegrity)
{
    HalfEdgeMesh hem = quadBoxHE();
    ASSERT_TRUE(hem.bevelVertex(0, 0.2f));
    const auto r = hem.checkIntegrity();
    EXPECT_TRUE(r.ok) << r.reason;
}

// pokeFace fans a face into triangles around a new centroid vertex; it must
// stay integrity-clean and closed (χ-neutral: +1 vertex, +2n half-edges,
// face count grows by n-1 ⇒ Δχ = 0). Regression for the old 6n-half-edge
// addEdgePair misuse that orphaned the perimeter edges on a dead face.
TEST(HEMEulerIntegrity, EverySuccessfulPokeFacePreservesIntegrity)
{
    const HalfEdgeMesh base = quadBoxHE();
    int pokes = 0;
    for (uint32_t f = 0; f < base.faceCount(); ++f) {
        HalfEdgeMesh hem = base;
        if (!hem.pokeFace(f)) continue;
        ++pokes;
        const auto r = hem.checkIntegrity();
        ASSERT_TRUE(r.ok) << "pokeFace " << f << ": " << r.reason;
        EXPECT_EQ(r.boundaryEdges, 0u) << "pokeFace opened a boundary, face " << f;
        EXPECT_EQ(closedEuler(r), 2u) << "pokeFace broke genus-0, face " << f;
    }
    EXPECT_GT(pokes, 0) << "no face accepted pokeFace — test is vacuous";
}

// connectVertices splits a face by a diagonal between two of its vertices; it
// must leave both sub-faces integrity-clean and the shell closed (χ-neutral:
// +1 edge, +1 face ⇒ Δχ = 0). Regression for the old cross-edge wiring that let
// face A's cycle spill into face B.
TEST(HEMEulerIntegrity, EverySuccessfulConnectVerticesPreservesIntegrity)
{
    const HalfEdgeMesh base = quadBoxHE();
    int connects = 0;
    for (uint32_t a = 0; a < base.vertexCount(); ++a) {
        for (uint32_t b = a + 1; b < base.vertexCount(); ++b) {
            HalfEdgeMesh hem = base;
            if (!hem.connectVertices(a, b)) continue;
            ++connects;
            const auto r = hem.checkIntegrity();
            ASSERT_TRUE(r.ok) << "connectVertices(" << a << "," << b << "): " << r.reason;
            EXPECT_EQ(r.boundaryEdges, 0u) << "connectVertices opened a boundary (" << a << "," << b << ")";
            EXPECT_EQ(closedEuler(r), 2u) << "connectVertices broke genus-0 (" << a << "," << b << ")";
        }
    }
    EXPECT_GT(connects, 0) << "no vertex pair accepted connectVertices — test is vacuous";
}

// insertEdgeLoop splits each edge of a loop and connects the new vertices,
// cutting the crossed faces — the result must stay integrity-clean and (on a
// closed solid) closed genus-0. Regression for the crossed split-edge twins and
// the dangling addEdgePair cross edges in splitFacesAlongLoop.
TEST(HEMEulerIntegrity, EverySuccessfulInsertEdgeLoopPreservesIntegrity)
{
    const HalfEdgeMesh base = quadBoxHE();
    int loops = 0;
    for (uint32_t e = 0; e < base.edgeCount(); ++e) {
        HalfEdgeMesh hem = base;
        if (!hem.insertEdgeLoop(e, 0.5f)) continue;
        ++loops;
        const auto r = hem.checkIntegrity();
        ASSERT_TRUE(r.ok) << "insertEdgeLoop seed " << e << ": " << r.reason;
        EXPECT_EQ(r.boundaryEdges, 0u) << "insertEdgeLoop opened a boundary, seed " << e;
        EXPECT_EQ(closedEuler(r), 2u) << "insertEdgeLoop broke genus-0, seed " << e;
    }
    EXPECT_GT(loops, 0) << "no seed accepted insertEdgeLoop — test is vacuous";
}

// extrudeFaces (keepOriginal) pushes a face out and skirts it with wall quads;
// on a closed solid the result stays closed genus-0 (the pushed face is the new
// cap). Regression for the old addEdgePair misuse that mis-twinned the walls.
TEST(HEMEulerIntegrity, EverySuccessfulExtrudeKeepPreservesIntegrity)
{
    const HalfEdgeMesh base = quadBoxHE();
    int extrudes = 0;
    for (uint32_t f = 0; f < base.faceCount(); ++f) {
        HalfEdgeMesh hem = base;
        if (!hem.extrudeFaces({f}, 0.5f, true)) continue;
        ++extrudes;
        const auto r = hem.checkIntegrity();
        ASSERT_TRUE(r.ok) << "extrude keep face " << f << ": " << r.reason;
        EXPECT_EQ(r.boundaryEdges, 0u) << "extrude keep opened a boundary, face " << f;
        EXPECT_EQ(closedEuler(r), 2u) << "extrude keep broke genus-0, face " << f;
    }
    EXPECT_GT(extrudes, 0) << "no face accepted extrudeFaces — test is vacuous";
}

// extrudeFaces(keepOriginal=false) discards the cap, leaving an open skirt; it
// must still be integrity-clean (boundary edges are expected, not a defect).
TEST(HEMEulerIntegrity, ExtrudeDiscardOriginalIsIntegrityClean)
{
    HalfEdgeMesh hem = quadBoxHE();
    ASSERT_TRUE(hem.extrudeFaces({0}, 0.5f, false));
    const auto r = hem.checkIntegrity();
    EXPECT_TRUE(r.ok) << r.reason;
    EXPECT_GT(r.boundaryEdges, 0u) << "discarding the cap should open a boundary";
}

}  // namespace nexus::geometry::testing
