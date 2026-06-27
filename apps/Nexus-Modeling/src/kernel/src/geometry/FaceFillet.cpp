#include <nexus/geometry/FaceFillet.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace nexus::geometry {

Mesh faceFillet(const Mesh& solid, uint32_t faceA, uint32_t faceB, const FaceFilletOptions& opts) noexcept {
    if(!solid.isValid()||opts.radius<=0.f) return solid;
    auto heOpt = HalfEdgeMesh::fromMesh(solid);
    if(!heOpt) return solid;
    HalfEdgeMesh he = *heOpt;
    if(faceA>=he.faceCount()||faceB>=he.faceCount()) return solid;

    // Find the common edge between faceA and faceB.
    uint32_t commonEdge = HalfEdgeMesh::kInvalid;
    uint32_t startHe = he.face(faceA).edge;
    if(startHe==HalfEdgeMesh::kInvalid) return solid;
    uint32_t heIdx = startHe;
    do {
        uint32_t twin = he.edge(heIdx).twin;
        if(twin!=HalfEdgeMesh::kInvalid && he.edge(twin).face==faceB) {
            commonEdge = heIdx; break;
        }
        heIdx = he.edge(heIdx).next;
    } while(heIdx!=startHe);

    if(commonEdge==HalfEdgeMesh::kInvalid) return solid;

    // Compute face normals and bisector for fillet direction.
    auto faceNormal = [&](uint32_t f)->Vec3{
        uint32_t fe = he.face(f).edge;
        Vec3 a=he.positions()[he.edge(fe).src];
        Vec3 b=he.positions()[he.edge(he.edge(fe).next).src];
        Vec3 c=he.positions()[he.edge(he.edge(he.edge(fe).next).next).src];
        Vec3 n=(b-a).cross(c-a); float len=n.length();
        return (len>1e-10f)?n*(1.f/len):Vec3{0,0,1};
    };

    Vec3 nA=faceNormal(faceA), nB=faceNormal(faceB);
    Vec3 bisector=nA+nB; float bLen=bisector.length();
    if(bLen<1e-10f) return solid;
    bisector=bisector*(1.f/bLen);

    // Check convex/concave.
    Vec3 pa=he.positions()[he.edge(commonEdge).src];
    uint32_t twin=he.edge(commonEdge).twin;
    Vec3 pb=he.positions()[he.edge(twin).src];
    Vec3 mid=(pa+pb)*0.5f;
    Vec3 offset=mid+bisector*opts.radius;
    if((offset-pa).dot(nA)<0.f||(offset-pb).dot(nB)<0.f) bisector=bisector*-1.f;

    // Split the common edge to create the fillet facet.
    uint32_t oldVc=he.vertexCount();
    if(!he.splitEdge(commonEdge)) return solid;
    if(oldVc>=he.vertexCount()) return solid;
    he.positions()[oldVc]=mid+bisector*opts.radius;

    return he.toMesh();
}

NurbsSurface extendSurface(const NurbsSurface& surf, const SurfaceExtensionOptions& opts) noexcept {
    if(!surf.isValid()) return {};
    int32_t nU=surf.controlPointCountU(), nV=surf.controlPointCountV();
    int32_t extU=opts.extendU?std::max(4,nU/4):0;
    int32_t extV=opts.extendU?0:std::max(4,nV/4);
    int32_t newNU=nU+extU, newNV=nV+extV;
    std::vector<Vec3> cps(static_cast<size_t>(newNU)*static_cast<size_t>(newNV));

    // Copy existing CPs and extrapolate new ones.
    for(int32_t i=0;i<newNU;++i) for(int32_t j=0;j<newNV;++j) {
        int32_t si=std::min(i,nU-1), sj=std::min(j,nV-1);
        Vec3 pt=surf.evaluate(
            surf.domainU().first+(surf.domainU().second-surf.domainU().first)*static_cast<float>(si)/static_cast<float>(nU-1),
            surf.domainV().first+(surf.domainV().second-surf.domainV().first)*static_cast<float>(sj)/static_cast<float>(nV-1));
        if(i>=nU) { Vec3 tan=surf.derivativeU(surf.domainU().second-0.01f,surf.domainV().first); pt=pt+tan*opts.distance*static_cast<float>(i-nU+1)/static_cast<float>(extU); }
        if(j>=nV) { Vec3 tan=surf.derivativeV(surf.domainU().first,surf.domainV().second-0.01f); pt=pt+tan*opts.distance*static_cast<float>(j-nV+1)/static_cast<float>(extV); }
        cps[static_cast<size_t>(i*newNV+j)]=pt;
    }

    int32_t degU=std::min(3,newNU-1),degV=std::min(3,newNV-1); degU=std::max(degU,1);degV=std::max(degV,1);
    std::vector<float> kU(static_cast<size_t>(newNU+degU+1)),kV(static_cast<size_t>(newNV+degV+1));
    for(int32_t j=0;j<=degU;++j){kU[static_cast<size_t>(j)]=0.f;kU[kU.size()-1-static_cast<size_t>(j)]=1.f;}
    for(int32_t j=1;j<newNU-degU;++j) kU[static_cast<size_t>(degU+j)]=static_cast<float>(j)/static_cast<float>(newNU-degU);
    for(int32_t j=0;j<=degV;++j){kV[static_cast<size_t>(j)]=0.f;kV[kV.size()-1-static_cast<size_t>(j)]=1.f;}
    for(int32_t j=1;j<newNV-degV;++j) kV[static_cast<size_t>(degV+j)]=static_cast<float>(j)/static_cast<float>(newNV-degV);
    return NurbsSurface(degU,degV,std::move(kU),std::move(kV),std::move(cps),newNU,newNV);
}

Mesh offsetFaceWithDraft(const HalfEdgeMesh& mesh, uint32_t face, const FaceOffsetOptions& opts) noexcept {
    if (face >= mesh.faceCount()) return {};
    Mesh result;
    auto positions = mesh.positions();
    const auto& heFace = mesh.face(face);

    std::vector<Vec3> faceVerts;
    uint32_t heIdx = heFace.edge;
    uint32_t start = heIdx;
    if (start == HalfEdgeMesh::kInvalid) return {};

    do {
        faceVerts.push_back(positions[mesh.edge(heIdx).src]);
        heIdx = mesh.edge(heIdx).next;
    } while (heIdx != start && heIdx != HalfEdgeMesh::kInvalid);

    if (faceVerts.size() < 3) return {};

    Vec3 e1{faceVerts[1].x - faceVerts[0].x, faceVerts[1].y - faceVerts[0].y, faceVerts[1].z - faceVerts[0].z};
    Vec3 e2{faceVerts[2].x - faceVerts[0].x, faceVerts[2].y - faceVerts[0].y, faceVerts[2].z - faceVerts[0].z};
    Vec3 normal{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
    float nLen = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (nLen < 1e-10f) return {};
    normal = Vec3{normal.x / nLen, normal.y / nLen, normal.z / nLen};

    float draftAngleRad = opts.draftAngleDeg * std::numbers::pi_v<float> / 180.0f;
    float offsetDist = opts.offset;
    float draftScale = std::tan(draftAngleRad) * offsetDist;

    for (auto& v : faceVerts) {
        v = Vec3{
            v.x + normal.x * offsetDist + normal.x * draftScale,
            v.y + normal.y * offsetDist + normal.y * draftScale,
            v.z + normal.z * offsetDist + normal.z * draftScale,
        };
    }

    Mesh outMesh;
    outMesh.attributes().setPositions(faceVerts);
    for (size_t i = 0; i + 2 < faceVerts.size(); ++i) {
        Face f;
        f.indices = {0, static_cast<uint32_t>(i + 1), static_cast<uint32_t>(i + 2)};
        outMesh.topology().addFace(f);
    }

    return outMesh;
}

ImprintResult imprintCurveOnMesh(const Mesh& mesh, const NurbsCurve& curve, const Vec3& projDir) noexcept {
    ImprintResult result;
    if (!mesh.isValid()) return result;

    const auto& positions = mesh.attributes().positions();
    const auto& topology = mesh.topology();

    float dx = projDir.x, dy = projDir.y, dz = projDir.z;
    float dLen = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dLen < 1e-10f) { dx = 0; dy = 0; dz = 1; }
    else { dx /= dLen; dy /= dLen; dz /= dLen; }

    const int curveSamples = 64;
    auto [domainMin, domainMax] = curve.domain();
    for (int s = 0; s <= curveSamples; ++s) {
        float t = static_cast<float>(s) / static_cast<float>(curveSamples);
        Vec3 curvePt = curve.evaluate(domainMin + t * (domainMax - domainMin));

        for (size_t fi = 0; fi < topology.faceCount(); ++fi) {
            const auto& face = topology.face(fi);
            if (face.indices.size() < 3) continue;

            uint32_t i0 = face.indices[0];
            uint32_t i1 = face.indices[1];
            uint32_t i2 = face.indices[2];
            if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) continue;

            const Vec3& v0 = positions[i0];
            const Vec3& v1 = positions[i1];
            const Vec3& v2 = positions[i2];

            Vec3 e1{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
            Vec3 e2{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
            Vec3 fn{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
            float fnLen = std::sqrt(fn.x * fn.x + fn.y * fn.y + fn.z * fn.z);
            if (fnLen < 1e-10f) continue;
            fn = Vec3{fn.x / fnLen, fn.y / fnLen, fn.z / fnLen};

            float denom = fn.x * dx + fn.y * dy + fn.z * dz;
            if (std::abs(denom) < 1e-10f) continue;

            float nd = fn.x * (v0.x - curvePt.x) + fn.y * (v0.y - curvePt.y) + fn.z * (v0.z - curvePt.z);
            float hitT = nd / denom;

            Vec3 hitPt{
                curvePt.x + dx * hitT,
                curvePt.y + dy * hitT,
                curvePt.z + dz * hitT,
            };

            Vec3 v0h{hitPt.x - v0.x, hitPt.y - v0.y, hitPt.z - v0.z};
            float d00 = e1.x * v0h.x + e1.y * v0h.y + e1.z * v0h.z;
            float d01 = e2.x * v0h.x + e2.y * v0h.y + e2.z * v0h.z;
            float d11 = e1.x * e1.x + e1.y * e1.y + e1.z * e1.z;
            float d12 = e1.x * e2.x + e1.y * e2.y + e1.z * e2.z;
            float d22 = e2.x * e2.x + e2.y * e2.y + e2.z * e2.z;
            float invDenom = 1.0f / (d11 * d22 - d12 * d12 + 1e-10f);
            float u = (d22 * d00 - d12 * d01) * invDenom;
            float v = (d11 * d01 - d12 * d00) * invDenom;

            if (u >= 0 && v >= 0 && u + v <= 1) {
                result.newEdges.push_back(static_cast<uint32_t>(fi));
                result.success = true;
                break;
            }
        }
    }

    return result;
}
}
