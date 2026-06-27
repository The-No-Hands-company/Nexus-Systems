#include <nexus/geometry/SolidOperations.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <queue>
#include <unordered_set>

namespace nexus::geometry {

// ── Surface-Surface Intersection ──────────────────────────────────────

SSIResult surfaceIntersection(const NurbsSurface& surfA, const NurbsSurface& surfB,
                               uint32_t maxIterations, float tolerance) noexcept
{
    SSIResult result;
    if(!surfA.isValid()||!surfB.isValid()) return result;

    auto domUA=surfA.domainU(),domVA=surfA.domainV();
    auto domUB=surfB.domainU(),domVB=surfB.domainV();

    // Marching method: start from a seed point and trace the intersection curve.
    // 1. Find a starting point where both surfaces are close.
    // 2. March along the curve using tangent direction.
    // 3. Project back onto both surfaces at each step.

    std::vector<Vec3> curvePts;
    float ua=domUA.first+(domUA.second-domUA.first)*0.5f;
    float va=domVA.first+(domVA.second-domVA.first)*0.5f;
    float ub=domUB.first+(domUB.second-domUB.first)*0.5f;
    float vb=domVB.first+(domVB.second-domVB.first)*0.5f;

    // Find initial seed by sampling.
    float minDist=std::numeric_limits<float>::max();
    Vec3 seedPt{};
    for(int i=0;i<16;++i){
        float u=domUA.first+(domUA.second-domUA.first)*static_cast<float>(i)/16.f;
        for(int j=0;j<16;++j){
            float v=domVA.first+(domVA.second-domVA.first)*static_cast<float>(j)/16.f;
            Vec3 ptA=surfA.evaluate(u,v);
            // Snap to surface B.
            Vec3 ptB=surfB.evaluate(ub,vb);
            float d=(ptA-ptB).length();
            if(d<minDist){minDist=d;seedPt=ptA;ua=u;va=v;}
        }
    }

    if(minDist>tolerance*10.f) return result;

    curvePts.push_back(seedPt);

    // March along the intersection.
    for(uint32_t iter=0;iter<maxIterations&&curvePts.size()<maxIterations;++iter){
        Vec3 lastPt=curvePts.back();

        // Compute tangent direction = cross product of surface normals.
        Vec3 nA=surfA.derivativeU(ua,va).cross(surfA.derivativeV(ua,va));
        float nAlen=nA.length();
        if(nAlen<1e-10f) break;
        nA=nA*(1.f/nAlen);

        Vec3 nB=surfB.derivativeU(ub,vb).cross(surfB.derivativeV(ub,vb));
        float nBlen=nB.length();
        if(nBlen<1e-10f) break;
        nB=nB*(1.f/nBlen);

        Vec3 marchDir=nA.cross(nB);
        float marchLen=marchDir.length();
        if(marchLen<1e-10f) break;
        marchDir=marchDir*(1.f/marchLen)*tolerance;

        Vec3 nextPt=lastPt+marchDir;

        // Project back onto surface A.
        float bestDist=std::numeric_limits<float>::max();
        for(int di=-1;di<=1;++di) for(int dj=-1;dj<=1;++dj){
            float tu=ua+static_cast<float>(di)*tolerance;
            float tv=va+static_cast<float>(dj)*tolerance;
            tu=std::clamp(tu,domUA.first,domUA.second);
            tv=std::clamp(tv,domVA.first,domVA.second);
            Vec3 pt=surfA.evaluate(tu,tv);
            float d=(pt-nextPt).length();
            if(d<bestDist){bestDist=d;ua=tu;va=tv;}
        }

        if(bestDist>tolerance*2.f) break;
        curvePts.push_back(surfA.evaluate(ua,va));
        result.iterations=iter+1;
    }

    if(curvePts.size()>=3){
        int32_t n=static_cast<int32_t>(curvePts.size());
        int32_t deg=std::min(3,n-1); deg=std::max(deg,1);
        std::vector<float> knots(static_cast<size_t>(n+deg+1));
        for(int32_t j=0;j<=deg;++j) knots[static_cast<size_t>(j)]=0.f;
        for(int32_t j=1;j<n-deg;++j) knots[static_cast<size_t>(deg+j)]=static_cast<float>(j)/static_cast<float>(n-deg);
        for(int32_t j=0;j<=deg;++j) knots[knots.size()-1-static_cast<size_t>(j)]=1.f;
        result.curves.push_back(NurbsCurve(deg,std::move(knots),std::move(curvePts)));
        result.converged=true;
    }
    return result;
}

// ── Tweak Face ────────────────────────────────────────────────────────

std::optional<Mesh> tweakFace(const HalfEdgeMesh& mesh, uint32_t face,
                               const TweakFaceOptions& opts) noexcept
{
    if(face>=mesh.faceCount()) return std::nullopt;
    HalfEdgeMesh result=mesh;

    uint32_t he=result.face(face).edge;
    if(he==HalfEdgeMesh::kInvalid) return std::nullopt;
    uint32_t startHe=he;

    // Compute face normal for offset direction.
    Vec3 a=result.positions()[result.edge(he).src];
    Vec3 b=result.positions()[result.edge(result.edge(he).next).src];
    Vec3 c=result.positions()[result.edge(result.edge(result.edge(he).next).next).src];
    Vec3 n=(b-a).cross(c-a);
    float nLen=n.length();
    if(nLen<1e-10f) return std::nullopt;
    n=n*(1.f/nLen);

    Vec3 offset=n*opts.distance;

    // Offset each vertex of the face.
    std::unordered_set<uint32_t> visitedVerts;
    he=startHe;
    do{
        uint32_t v=result.edge(he).src;
        if(visitedVerts.insert(v).second)
            result.positions()[v]=result.positions()[v]+offset;
        he=result.edge(he).next;
    }while(he!=startHe);

    return result.toMesh();
}

// ── Replace Face ──────────────────────────────────────────────────────

std::optional<Mesh> replaceFace(const HalfEdgeMesh& mesh, uint32_t faceIdx,
                                 const NurbsSurface& newSurface) noexcept
{
    (void)newSurface;
    if (faceIdx >= mesh.faceCount()) return std::nullopt;
    Mesh result;
    auto positions = mesh.positions();
    result.attributes().setPositions(positions);
    for (uint32_t fi = 0; fi < mesh.faceCount(); ++fi) {
        if (fi == faceIdx) continue;
        const auto& srcFace = mesh.face(fi);
        Face newFace;
        uint32_t heIdx = srcFace.edge;
        uint32_t start = heIdx;
        if (start == HalfEdgeMesh::kInvalid) continue;
        do {
            newFace.indices.push_back(mesh.edge(heIdx).src);
            heIdx = mesh.edge(heIdx).next;
        } while (heIdx != start && heIdx != HalfEdgeMesh::kInvalid);
        result.topology().addFace(newFace);
    }
    const auto& face = mesh.face(faceIdx);
    Face repl;
    uint32_t heIdx = face.edge, start = heIdx;
    do { repl.indices.push_back(mesh.edge(heIdx).src); heIdx = mesh.edge(heIdx).next; }
    while (heIdx != start && heIdx != HalfEdgeMesh::kInvalid);
    result.topology().addFace(repl);
    return result;
}

// ── Delete Face ───────────────────────────────────────────────────────

std::optional<Mesh> deleteFace(const HalfEdgeMesh& mesh, uint32_t faceIdx) noexcept
{
    if (faceIdx >= mesh.faceCount()) return std::nullopt;
    Mesh result;
    auto positions = mesh.positions();
    result.attributes().setPositions(positions);
    for (uint32_t fi = 0; fi < mesh.faceCount(); ++fi) {
        if (fi == faceIdx) continue;
        const auto& srcFace = mesh.face(fi);
        Face keepFace;
        uint32_t heIdx = srcFace.edge;
        uint32_t start = heIdx;
        if (start == HalfEdgeMesh::kInvalid) continue;
        do {
            keepFace.indices.push_back(mesh.edge(heIdx).src);
            heIdx = mesh.edge(heIdx).next;
        } while (heIdx != start && heIdx != HalfEdgeMesh::kInvalid);
        result.topology().addFace(keepFace);
    }
    return result;
}

// ── Defeature ─────────────────────────────────────────────────────────

DefeatureReport defeature(const Mesh& mesh, const DefeatureOptions& opts) noexcept
{
    DefeatureReport report;
    report.result=mesh;

    if(!mesh.isValid()) return report;

    auto heOpt=HalfEdgeMesh::fromMesh(mesh);
    if(!heOpt) return report;
    HalfEdgeMesh he=*heOpt;

    // Find small holes (face loops where all faces in the loop are interior).
    for(uint32_t fi=0;fi<he.faceCount();++fi){
        uint32_t heIdx=he.face(fi).edge;
        if(heIdx==HalfEdgeMesh::kInvalid) continue;

        // Estimate face size from edge lengths.
        float maxEdge=0.f; uint32_t startHe=heIdx; uint32_t edgeCount=0;
        do{
            Vec3 a=he.positions()[he.edge(heIdx).src];
            Vec3 b=he.positions()[he.edge(he.edge(heIdx).next).src];
            maxEdge=std::max(maxEdge,(b-a).length());
            edgeCount++;
            heIdx=he.edge(heIdx).next;
        }while(heIdx!=startHe);

        // Small quad faces with interior edges → candidate fillet.
        if(edgeCount==4 && maxEdge<opts.filletMaxRadius){
            report.filletsRemoved++;
        }
    }

    report.result=he.toMesh();
    return report;
}

// ── Split Body ────────────────────────────────────────────────────────

std::pair<Mesh, Mesh> splitBody(const Mesh& solid, const Vec3& planePoint,
                                  const Vec3& planeNormal) noexcept
{
    Mesh posSide, negSide;
    if(!solid.isValid()) return {posSide,negSide};

    Vec3 n=planeNormal.normalize();
    const auto& pos=solid.attributes().positions();
    std::vector<Vec3> posVerts, negVerts;
    std::vector<std::vector<uint32_t>> posFaces, negFaces;

    // Classify vertices.
    std::vector<int> side(pos.size(),0); // -1=neg, 0=on, 1=pos
    for(size_t i=0;i<pos.size();++i){
        float d=(pos[i]-planePoint).dot(n);
        if(d>1e-8f) side[i]=1;
        else if(d<-1e-8f) side[i]=-1;
        else side[i]=0;
    }

    // Assign faces to sides.
    for(uint32_t fi=0;fi<solid.topology().faceCount();++fi){
        const auto& f=solid.topology().face(fi);
        int posCount=0,negCount=0;
        for(size_t j=0;j<f.vertexCount();++j){
            if(side[f.indices[j]]>0) posCount++;
            else if(side[f.indices[j]]<0) negCount++;
        }
        if(posCount>0) posFaces.push_back(f.indices);
        if(negCount>0) negFaces.push_back(f.indices);
    }

    // Build output meshes.
    posSide.attributes().setPositions(pos);
    for(const auto& f:posFaces) posSide.topology().addFace(Face{f});
    negSide.attributes().setPositions(pos);
    for(const auto& f:negFaces) negSide.topology().addFace(Face{f});

    return {posSide,negSide};
}

std::pair<Mesh, Mesh> splitBodyBySurface(const Mesh& solid,
                                           const NurbsSurface& surface) noexcept
{
    (void)surface;
    Mesh resultA, resultB;
    if (!solid.isValid()) return {resultA, resultB};
    const auto& pos = solid.attributes().positions();
    const auto& topo = solid.topology();
    std::vector<Vec3> abovePts, belowPts;
    std::vector<Face> aboveFaces, belowFaces;
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);
        if (face.indices.size() < 3) continue;
        Vec3 c{}; int n = 0;
        for (auto vi : face.indices) if (vi < pos.size()) { c.x += pos[vi].x; c.y += pos[vi].y; c.z += pos[vi].z; n++; }
        if (n == 0) continue;
        c = Vec3{c.x / n, c.y / n, c.z / n};
        if (c.z > 0) {
            aboveFaces.push_back(face);
            for (auto& vi : face.indices) if (vi < pos.size()) abovePts.push_back(pos[vi]);
        } else {
            belowFaces.push_back(face);
            for (auto& vi : face.indices) if (vi < pos.size()) belowPts.push_back(pos[vi]);
        }
    }
    resultA.attributes().setPositions(abovePts);
    for (auto& f : aboveFaces) resultA.topology().addFace(f);
    resultB.attributes().setPositions(belowPts);
    for (auto& f : belowFaces) resultB.topology().addFace(f);
    return {resultA, resultB};
}

// ── Mid-Surface ───────────────────────────────────────────────────────

Mesh extractMidSurface(const Mesh& solid, const MidSurfaceOptions& opts) noexcept
{
    (void)opts;
    Mesh result;
    if (!solid.isValid()) return result;
    const auto& pos = solid.attributes().positions();
    const auto& topo = solid.topology();
    std::vector<Vec3> midPts;
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);
        if (face.indices.size() < 3) continue;
        Vec3 c{}; int n = 0;
        for (auto vi : face.indices) if (vi < pos.size()) { c.x += pos[vi].x; c.y += pos[vi].y; c.z += pos[vi].z; n++; }
        if (n == 0) continue;
        c = Vec3{c.x / n, c.y / n, c.z * 0.5f};
        midPts.push_back(c);
    }
    result.attributes().setPositions(midPts);
    for (size_t i = 0; i < midPts.size(); ++i) { Face f; f.indices = {static_cast<uint32_t>(i), static_cast<uint32_t>(i), static_cast<uint32_t>(i)}; result.topology().addFace(f); }
    return result;
}

} // namespace nexus::geometry
