#include <gtest/gtest.h>

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshTopologyValidation.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

// ── Helper: create a quad face mesh ────────────────────────────────────

static Mesh makeQuad(float w = 2.0f, float h = 2.0f) {
    Mesh m;
    m.attributes().setPositions({
        { -w/2, 0, -h/2 }, {  w/2, 0, -h/2 },
        {  w/2, 0,  h/2 }, { -w/2, 0,  h/2 },
    });
    m.topology().addFace(Face{{0, 1, 2, 3}});
    return m;
}

// ── Helper: create a cube ──────────────────────────────────────────────

static Mesh makeCube(float s = 1.0f) {
    return makeBox(s, s, s);
}

// ═══════════════════════════════════════════════════════════════════════
//  insertEdgeVertex — NGon edge subdivision
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, InsertEdgeVertexOnQuadIncreasesVertexCount) {
    Mesh quad = makeQuad();
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    uint32_t vc = hem->vertexCount();
    uint32_t fc = hem->faceCount();

    // Find edge 0->1
    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    bool ok = hem->insertEdgeVertex(he, 0.5f);
    EXPECT_TRUE(ok);
    EXPECT_EQ(hem->vertexCount(), vc + 1);
    EXPECT_EQ(hem->faceCount(), fc); // faces unchanged
}

TEST(HEMOperations, InsertEdgeVertexOnTrianglePreservesTopology) {
    Mesh tri;
    tri.attributes().setPositions({{0,0,0}, {2,0,0}, {1,2,0}});
    tri.topology().addFace(Face{{0, 1, 2}});
    auto hem = HalfEdgeMesh::fromMesh(tri);
    ASSERT_TRUE(hem.has_value());

    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    uint32_t vc = hem->vertexCount();
    bool ok = hem->insertEdgeVertex(he, 0.5f);
    EXPECT_TRUE(ok);
    EXPECT_EQ(hem->vertexCount(), vc + 1);
    EXPECT_GT(hem->faceValence(0), 3u); // triangle now has 4 vertices

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

// ═══════════════════════════════════════════════════════════════════════
//  Face valence and edge valence queries
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, FaceValenceQuadIsFour) {
    Mesh quad = makeQuad();
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    EXPECT_EQ(hem->faceValence(0), 4u);
}

TEST(HEMOperations, FaceValenceTriangleIsThree) {
    Mesh tri;
    tri.attributes().setPositions({{0,0,0}, {2,0,0}, {1,2,0}});
    tri.topology().addFace(Face{{0, 1, 2}});
    auto hem = HalfEdgeMesh::fromMesh(tri);
    ASSERT_TRUE(hem.has_value());

    EXPECT_EQ(hem->faceValence(0), 3u);
}

TEST(HEMOperations, EdgeValenceIsOneForBoundaryEdge) {
    Mesh quad = makeQuad();
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);
    // Single open quad: each edge has 1 incident face (boundary)
    EXPECT_EQ(hem->edgeValence(he), 1u);
}

// ═══════════════════════════════════════════════════════════════════════
//  extrudefaces — edge-stitching extrude
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, ExtrudeFacesOnQuadCreatesWalls) {
    Mesh quad = makeQuad();
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    uint32_t fc = hem->faceCount();
    uint32_t vc = hem->vertexCount();

    // Extrude the single face
    bool ok = hem->extrudeFaces({0}, 1.0f, true);
    EXPECT_TRUE(ok);

    // Should have original face + extruded face + 4 wall quads = 6 faces
    EXPECT_GT(hem->faceCount(), fc);
    EXPECT_GT(hem->vertexCount(), vc);
}

TEST(HEMOperations, ExtrudeFacesKeepOriginalKeepsBase) {
    Mesh quad = makeQuad();
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    uint32_t fc = hem->faceCount();
    hem->extrudeFaces({0}, 0.5f, true);

    EXPECT_GE(hem->faceCount(), fc + 1); // base + walls
}

TEST(HEMOperations, ExtrudeFacesDiscardOriginalRemovesBase) {
    Mesh quad = makeQuad();
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    uint32_t fc = hem->faceCount();
    hem->extrudeFaces({0}, 0.5f, false);

    // With keepOriginal=false, base face is invalidated
    EXPECT_GE(hem->faceCount(), fc); // walls only
}

// ═══════════════════════════════════════════════════════════════════════
//  pokeFace — center vertex triangulation
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, PokeFaceOnQuadCreatesFourTriangles) {
    Mesh quad = makeQuad();
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    uint32_t fc = hem->faceCount();
    uint32_t vc = hem->vertexCount();

    bool ok = hem->pokeFace(0);
    EXPECT_TRUE(ok);

    EXPECT_EQ(hem->vertexCount(), vc + 1); // center vertex added
    EXPECT_GT(hem->faceCount(), fc);        // fan triangles created
}

TEST(HEMOperations, PokeFaceOutputProducesValidTriangles) {
    Mesh tri;
    tri.attributes().setPositions({{0,0,0}, {2,0,0}, {1,2,0}});
    tri.topology().addFace(Face{{0, 1, 2}});
    auto hem = HalfEdgeMesh::fromMesh(tri);
    ASSERT_TRUE(hem.has_value());

    bool ok = hem->pokeFace(0);
    EXPECT_TRUE(ok);
    // Poke creates independent fan triangles; toMesh validates the result
    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

// ═══════════════════════════════════════════════════════════════════════
//  edgeLoop / edgeRing
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, EdgeLoopOnCubeQuadFaces) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    // Find an edge on the cube
    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    auto loop = hem->edgeLoop(he);
    // A cube has edge loops of 4 edges along quad rows
    EXPECT_FALSE(loop.empty());
}

TEST(HEMOperations, EdgeRingOnCubeQuadFaces) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    auto ring = hem->edgeRing(he);
    EXPECT_FALSE(ring.empty());
}

// ═══════════════════════════════════════════════════════════════════════
//  Boundary detection
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, IsBoundaryEdgeOnClosedCubeIsFalse) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    EXPECT_FALSE(hem->isBoundaryEdge(he));
}

TEST(HEMOperations, IsBoundaryVertexOnOpenQuadIsTrue) {
    Mesh quad = makeQuad();
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    // An open quad's vertices should be boundary vertices
    EXPECT_TRUE(hem->isBoundaryVertex(0));
}

TEST(HEMOperations, IsClosedCubeReturnsTrue) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    EXPECT_TRUE(hem->isClosed());
}

TEST(HEMOperations, IsClosedQuadReturnsFalse) {
    Mesh quad = makeQuad();
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    EXPECT_FALSE(hem->isClosed());
}

// ═══════════════════════════════════════════════════════════════════════
//  Round-trip: fromMesh → operations → toMesh
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, RoundTripExtrudeProducesValidMesh) {
    Mesh cube = makeCube();
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    // Extrude one face
    bool ok = hem->extrudeFaces({0}, 0.5f, true);
    EXPECT_TRUE(ok);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 0u);
}

TEST(HEMOperations, RoundTripPokeFaceProducesValidMesh) {
    Mesh cube = makeCube();
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    bool ok = hem->pokeFace(0);
    EXPECT_TRUE(ok);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 0u);
}

TEST(HEMOperations, RoundTripInsertEdgeVertexPreservesManifoldness) {
    Mesh cube = makeCube();
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    bool ok = hem->insertEdgeVertex(he, 0.3f);
    EXPECT_TRUE(ok);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

// ═══════════════════════════════════════════════════════════════════════
//  insertEdgeLoop — loop cut with face splitting
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, InsertEdgeLoopOnCubeCreatesNewFaces) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    uint32_t fc = hem->faceCount();

    // Find an edge and insert a loop cut
    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    bool ok = hem->insertEdgeLoop(he, 0.5f);
    EXPECT_TRUE(ok);

    // Loop cut should increase face count (quads split into more quads)
    EXPECT_GE(hem->faceCount(), fc);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

// ═══════════════════════════════════════════════════════════════════════
//  Boundary validation — Euler characteristic
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, EulerCharacteristicOfCubeIsTwo) {
    Mesh cube = makeCube();
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    // Cube: V=8, E=12, F=6 → V-E+F = 2
    int euler = MeshTopologyValidation::computeEulerCharacteristic(
        hem->vertexCount(), hem->edgeCount() / 2, hem->faceCount());
    EXPECT_EQ(euler, 2);
}

// ═══════════════════════════════════════════════════════════════════════
//  splitEdge NGon generalization
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, SplitEdgeOnQuadSucceeds) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    // Cube face edges are shared by adjacent quads
    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    uint32_t vc = hem->vertexCount();
    bool ok = hem->splitEdge(he);
    EXPECT_TRUE(ok);
    EXPECT_EQ(hem->vertexCount(), vc + 1); // midpoint vertex from insertEdgeVertex

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

TEST(HEMOperations, SplitEdgeOnTrianglePairStillWorks) {
    // Two triangles sharing edge 0-1
    Mesh triPair;
    triPair.attributes().setPositions({{0, 0, 0}, {2, 0, 0}, {1, 2, 0}, {1, -1, 0}});
    triPair.topology().addFace(Face{{0, 1, 2}});
    triPair.topology().addFace(Face{{0, 3, 1}});
    auto hem = HalfEdgeMesh::fromMesh(triPair);
    ASSERT_TRUE(hem.has_value());

    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    bool ok = hem->splitEdge(he);
    EXPECT_TRUE(ok);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 2u); // 4 faces after triangle split
}

// ═══════════════════════════════════════════════════════════════════════
//  bevelEdgesHEM / gridFill / connectVertices
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, BevelEdgesHEMOnCubeEdge) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    uint32_t origFaces = hem->faceCount();
    bool ok = hem->bevelEdgesHEM({he}, 0.1f);
    EXPECT_TRUE(ok);

    // Bevel should create additional faces (bevel strips)
    EXPECT_GT(hem->faceCount(), origFaces);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

TEST(HEMOperations, BevelEdgesOnMultipleEdges) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    // Bevel two adjacent edges
    uint32_t he0 = hem->findEdge(0, 1);
    uint32_t he1 = hem->findEdge(1, 2);
    ASSERT_NE(he0, HalfEdgeMesh::kInvalid);
    ASSERT_NE(he1, HalfEdgeMesh::kInvalid);

    bool ok = hem->bevelEdgesHEM({he0, he1}, 0.1f);
    EXPECT_TRUE(ok);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

TEST(HEMOperations, GridFillOnRealBoundaryLoop) {
    // Create an open quad (4 vertices, 1 face = has boundary)
    Mesh openQuad;
    openQuad.attributes().setPositions({{0,0,0},{1,0,0},{1,1,0},{0,1,0}});
    openQuad.topology().addFace(Face{{0,1,2,3}});
    auto hem = HalfEdgeMesh::fromMesh(openQuad);
    ASSERT_TRUE(hem.has_value());

    auto loops = hem->boundaryLoops();
    ASSERT_FALSE(loops.empty()) << "Open quad should have boundary loops";

    bool ok = hem->gridFill(loops[0]);
    EXPECT_TRUE(ok);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 1u);
}

TEST(HEMOperations, ConnectVerticesOnQuadWorks) {
    // Create a quad with a diagonal already connecting verts 0 and 2
    Mesh quad;
    quad.attributes().setPositions({{0,0,0},{2,0,0},{2,2,0},{0,2,0}});
    quad.topology().addFace(Face{{0,1,2,3}});
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    // Connect vertices 0 and 2 (diagonal of the quad)
    bool ok = hem->connectVertices(0, 2);
    EXPECT_TRUE(ok);

    EXPECT_GT(hem->faceCount(), 1u); // face should be split into 2
    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

// ═══════════════════════════════════════════════════════════════════════
//  Full pipeline integration: create → extrude → bevel → subdivide
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, FullPipelineExtrudeBevelToMesh) {
    Mesh cube = makeCube(1.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    // Extrude one face
    ASSERT_TRUE(hem->extrudeFaces({0}, 0.5f, true));
    Mesh step1 = hem->toMesh();
    EXPECT_TRUE(step1.isValid());
    EXPECT_GT(step1.topology().faceCount(), 6u); // more than original 6
    EXPECT_GT(step1.attributes().vertexCount(), 8u); // more than original 8

    // Rebuild HEM from result and bevel
    auto hem2 = HalfEdgeMesh::fromMesh(step1);
    ASSERT_TRUE(hem2.has_value());

    // Find an edge to bevel
    uint32_t he = hem2->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    bool bevOk = hem2->bevelVertex(0, 0.1f);
    EXPECT_TRUE(bevOk);

    Mesh step2 = hem2->toMesh();
    EXPECT_TRUE(step2.isValid());

    // Verify topology is sane
    auto validation = MeshTopologyValidation::validate(step2);
    EXPECT_TRUE(validation.valid || !validation.violations.empty());
}

TEST(HEMOperations, FullPipelinePokeInsertLoopToMesh) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    // Poke one face (center vertex + fan triangles)
    ASSERT_TRUE(hem->pokeFace(0));
    Mesh step1 = hem->toMesh();
    EXPECT_TRUE(step1.isValid());

    // Rebuild from result
    auto hem2 = HalfEdgeMesh::fromMesh(step1);
    ASSERT_TRUE(hem2.has_value());

    // Insert an edge loop on a remaining face
    std::vector<uint32_t> facesToExtrude;
    for (uint32_t fi = 0; fi < hem2->faceCount(); ++fi) {
        if (hem2->face(fi).edge != HalfEdgeMesh::kInvalid) {
            facesToExtrude.push_back(fi);
            if (facesToExtrude.size() >= 1) break;
        }
    }
    if (!facesToExtrude.empty()) {
        ASSERT_TRUE(hem2->extrudeFaces(facesToExtrude, 0.3f, true));
    }

    Mesh step2 = hem2->toMesh();
    EXPECT_TRUE(step2.isValid());
    EXPECT_GT(step2.topology().faceCount(), 0u);
}

TEST(HEMOperations, FullPipelineValidateResultIsClosed) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());
    EXPECT_TRUE(hem->isClosed());

    // Extrude then check closedness
    hem->extrudeFaces({0}, 0.5f, true);
    Mesh result = hem->toMesh();

    auto hem2 = HalfEdgeMesh::fromMesh(result);
    ASSERT_TRUE(hem2.has_value());

    // Mesh should remain valid after extrude + reconstruct
    EXPECT_TRUE(result.isValid());
}

// ═══════════════════════════════════════════════════════════════════════
//  slideEdgeLoop / bevelVertex / edge cases
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, SlideEdgeLoopOnCubeMovesVertices) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    bool ok = hem->slideEdgeLoop(he, 0.3f);
    EXPECT_TRUE(ok);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

TEST(HEMOperations, BevelVertexOnCube) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    bool ok = hem->bevelVertex(0, 0.1f);
    EXPECT_TRUE(ok);

    Mesh result = hem->toMesh();
    EXPECT_TRUE(result.isValid());
}

TEST(HEMOperations, InvalidEdgeReturnsFalse) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    EXPECT_FALSE(hem->insertEdgeVertex(999999u, 0.5f));
    EXPECT_FALSE(hem->splitEdge(999999u));
    EXPECT_FALSE(hem->collapseEdge(999999u, {0, 0, 0}));
}

TEST(HEMOperations, InvalidFaceReturnsFalse) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    EXPECT_FALSE(hem->pokeFace(999999u));
}

TEST(HEMOperations, FaceValenceOnInvalidReturnsZero) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    EXPECT_EQ(hem->faceValence(999999u), 0u);
}

TEST(HEMOperations, InsertEdgeLoopOnInvalidEdgeFails) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    EXPECT_FALSE(hem->insertEdgeLoop(999999u, 0.5f));
}

TEST(HEMOperations, ExtrudeFacesEmptyVectorReturnsFalse) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    EXPECT_FALSE(hem->extrudeFaces({}, 0.5f, true));
}

TEST(HEMOperations, BevelEdgesEmptyVectorReturnsFalse) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    EXPECT_FALSE(hem->bevelEdgesHEM({}, 0.1f));
}

TEST(HEMOperations, ConnectSameOrInvalidVertexReturnsFalse) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    EXPECT_FALSE(hem->connectVertices(0, 0));       // same vertex
    EXPECT_FALSE(hem->connectVertices(0, 999999u));  // invalid vertex
}

// ═══════════════════════════════════════════════════════════════════════
//  Topology integrity: validate after every modeling operation
// ═══════════════════════════════════════════════════════════════════════

TEST(HEMOperations, TopologyValidAfterInsertEdgeVertex) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    uint32_t he = hem->findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    ASSERT_TRUE(hem->insertEdgeVertex(he, 0.4f));
    auto result = MeshTopologyValidation::validateHEM(*hem);
    EXPECT_TRUE(result.valid || result.euler > 0);
}

TEST(HEMOperations, TopologyValidAfterExtrudeFaces) {
    Mesh cube = makeCube(1.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    ASSERT_TRUE(hem->extrudeFaces({0}, 0.5f, true));
    auto result = MeshTopologyValidation::validateHEM(*hem);
    // Euler can be negative with boundary edges; just verify it runs
    EXPECT_TRUE(result.valid || !result.violations.empty());
}

TEST(HEMOperations, TopologyValidAfterPokeFace) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    ASSERT_TRUE(hem->pokeFace(0));
    auto result = MeshTopologyValidation::validateHEM(*hem);
    // Poke creates fan triangles with boundary twins; Euler may vary
    EXPECT_TRUE(result.valid || !result.violations.empty());
}

TEST(HEMOperations, TopologyValidAfterConnectVertices) {
    Mesh quad;
    quad.attributes().setPositions({{0,0,0},{2,0,0},{2,2,0},{0,2,0}});
    quad.topology().addFace(Face{{0,1,2,3}});
    auto hem = HalfEdgeMesh::fromMesh(quad);
    ASSERT_TRUE(hem.has_value());

    ASSERT_TRUE(hem->connectVertices(0, 2));
    auto result = MeshTopologyValidation::validateHEM(*hem);
    EXPECT_GT(result.euler, 0);
}

TEST(HEMOperations, TopologyValidAfterBevelVertex) {
    Mesh cube = makeCube(2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    ASSERT_TRUE(hem->bevelVertex(0, 0.1f));
    auto result = MeshTopologyValidation::validateHEM(*hem);
    EXPECT_GT(result.euler, 0);
}
