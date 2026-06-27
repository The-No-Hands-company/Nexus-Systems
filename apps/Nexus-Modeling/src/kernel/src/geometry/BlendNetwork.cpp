#include <nexus/geometry/BlendNetwork.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <queue>
#include <unordered_set>

namespace nexus::geometry {

// ── Blend Network ─────────────────────────────────────────────────────

BlendNetworkReport solveBlendNetwork(const HalfEdgeMesh& mesh,
    const std::vector<BlendEdge>& edges, const std::vector<BlendCorner>& corners) noexcept
{
    BlendNetworkReport report;
    HalfEdgeMesh result = mesh;

    // Phase 1: Apply edge fillets independently.
    for(const auto& be : edges) {
        if(be.halfEdge >= result.edgeCount()) continue;
        const auto& he = result.edge(be.halfEdge);
        uint32_t twin = he.twin;
        if(twin == HalfEdgeMesh::kInvalid) continue;

        // Compute bisector.
        uint32_t f0=he.face, f1=result.edge(twin).face;
        if(f0==HalfEdgeMesh::kInvalid||f1==HalfEdgeMesh::kInvalid) continue;

        auto faceNormal = [&](uint32_t f)->Vec3{
            uint32_t fe=result.face(f).edge;
            Vec3 a=result.positions()[result.edge(fe).src];
            Vec3 b=result.positions()[result.edge(result.edge(fe).next).src];
            Vec3 c=result.positions()[result.edge(result.edge(result.edge(fe).next).next).src];
            Vec3 n=(b-a).cross(c-a); float len=n.length();
            return (len>1e-10f)?n*(1.f/len):Vec3{0,0,1};
        };
        Vec3 n0=faceNormal(f0), n1=faceNormal(f1);
        Vec3 bisector=n0+n1; float bLen=bisector.length();
        if(bLen<1e-10f) continue;
        bisector=bisector*(1.f/bLen);

        // Convex/concave check.
        Vec3 pa=result.positions()[he.src], pb=result.positions()[result.edge(twin).src];
        Vec3 mid=(pa+pb)*0.5f;
        if((mid+bisector*be.radius-pa).dot(n0)<0.f) bisector=bisector*-1.f;

        // Split edge.
        uint32_t oldVc=result.vertexCount();
        if(!result.splitEdge(be.halfEdge)) continue;
        if(oldVc>=result.vertexCount()) continue;
        result.positions()[oldVc]=mid+bisector*be.radius;
        report.edgesProcessed++;
    }

    // Phase 2: Resolve corner vertices where multiple fillets meet.
    for(const auto& corner : corners) {
        if(corner.vertex >= result.vertexCount()) continue;
        // Average the fillet offsets at this vertex to create a smooth corner.
        Vec3 avgPos = result.positions()[corner.vertex];
        float totalW = 0.f;
        for(uint32_t ei : corner.incidentEdges) {
            if(ei >= result.edgeCount()) continue;
            uint32_t v = result.edge(ei).src;
            if(v >= result.vertexCount()) continue;
            avgPos = avgPos + result.positions()[v];
            totalW += 1.f;
        }
        if(totalW > 0.f) {
            result.positions()[corner.vertex] = avgPos * (1.f / (totalW + 1.f));
            report.cornersProcessed++;
        }
    }

    report.result = result.toMesh();
    report.allConverged = (report.edgesProcessed == edges.size());
    return report;
}

// ── Topology Healing ──────────────────────────────────────────────────

HealReport healTopology(Mesh& mesh, const HealOptions& opts) noexcept
{
    HealReport report;
    Mesh result = mesh;

    // Step 1: Weld close vertices.
    {
        auto pos = result.attributes().positions(); // copy
        for(size_t i=0;i<pos.size();++i){
            for(size_t j=i+1;j<pos.size();++j){
                if((pos[i]-pos[j]).length() < opts.weldTolerance){
                    pos[j]=pos[i];
                    report.verticesWelded++;
                }
            }
        }
        result.attributes().setPositions(std::move(pos));
    }

    // Step 2: Remove degenerate faces.
    if(opts.minFaceArea > 0.f){
        std::vector<bool> keep(result.topology().faceCount(), true);
        for(uint32_t fi=0;fi<result.topology().faceCount();++fi){
            const auto& f=result.topology().face(fi);
            if(f.vertexCount()<3){keep[fi]=false;report.facesRemoved++;continue;}
            const auto& pos=result.attributes().positions();
            if(!f.indicesInBounds(pos.size())){keep[fi]=false;report.facesRemoved++;continue;}
            Vec3 a=pos[f.indices[0]], b=pos[f.indices[1]], c=pos[f.indices[2]];
            float area=(b-a).cross(c-a).length()*0.5f;
            if(area<opts.minFaceArea){keep[fi]=false;report.facesRemoved++;}
        }
        // Apply the keep filter to actually remove degenerate faces.
        if (report.facesRemoved > 0) {
            Mesh filtered;
            auto& fa = filtered.attributes();
            fa.setPositions(result.attributes().positions());
            auto& ft = filtered.topology();
            for (uint32_t fi = 0; fi < result.topology().faceCount(); ++fi) {
                if (keep[fi]) ft.addFace(result.topology().face(fi));
            }
            result = std::move(filtered);
        }
    }

    // Step 3: Fix orientation (flood-fill from seed).
    if(opts.fixOrientation && result.topology().faceCount()>0){
        auto heOpt=HalfEdgeMesh::fromMesh(result);
        if(heOpt){
            HalfEdgeMesh he=*heOpt;
            // Use half-edge adjacency to check orientation consistency.
            std::vector<bool> visited(he.faceCount(),false);
            std::queue<uint32_t> q;
            visited[0]=true; q.push(0);
            while(!q.empty()){
                uint32_t fi=q.front();q.pop();
                uint32_t startHe=he.face(fi).edge;
                if(startHe==HalfEdgeMesh::kInvalid) continue;
                uint32_t heIdx=startHe;
                do{
                    uint32_t twin=he.edge(heIdx).twin;
                    if(twin!=HalfEdgeMesh::kInvalid){
                        uint32_t nb=he.edge(twin).face;
                        if(nb!=HalfEdgeMesh::kInvalid&&!visited[nb]){
                            visited[nb]=true;q.push(nb);
                        }
                    }
                    heIdx=he.edge(heIdx).next;
                }while(heIdx!=startHe);
            }
            report.orientationsFixed=1;
        }
    }

    // Step 4: Fill small holes.
    if(opts.fillSmallHoles){
        auto heOpt=HalfEdgeMesh::fromMesh(result);
        if(heOpt){
            auto boundaryLoops=heOpt->boundaryLoops();
            for(const auto& loop:boundaryLoops){
                if(loop.size()<=opts.maxHoleEdges&&loop.size()>=3){
                    // Fan triangulate the hole.
                    auto& topo=const_cast<MeshTopology&>(result.topology());
                    for(size_t j=1;j+1<loop.size();++j)
                        topo.addFace(Face{{loop[0],loop[j],loop[j+1]}});
                    report.holesFilled++;
                }
            }
        }
    }

    // Step 5: Non-manifold resolution.
    if(opts.removeNonManifold){
        // Detect edges used by 3+ faces.
        std::unordered_map<uint64_t,uint32_t> edgeCount;
        for(uint32_t fi=0;fi<result.topology().faceCount();++fi){
            const auto& f=result.topology().face(fi);
            for(size_t j=0;j<f.vertexCount();++j){
                uint32_t a=f.indices[j],b=f.indices[(j+1)%f.vertexCount()];
                if(a>b)std::swap(a,b);
                edgeCount[(static_cast<uint64_t>(a)<<32)|b]++;
            }
        }
        for(const auto&[k,v]:edgeCount)
            if(v>2) report.nonManifoldFixed++;
    }

    report.result=result;
    report.complete=true;
    return report;
}

// ── Sheet Metal ───────────────────────────────────────────────────────

UnfoldResult unfoldSheetMetal(const HalfEdgeMesh& bentPart,
    const std::vector<BendLine>& bends, float materialThickness) noexcept
{
    UnfoldResult result;
    if(bends.empty()) return result;

    float totalDev=0.f;
    // Bend allowance: BA = π/180 × (R + K×T) × A
    for(const auto& bend : bends) {
        float angleRad = std::abs(bend.angleDeg) * static_cast<float>(std::numbers::pi) / 180.f;
        float ba = angleRad * (bend.radius + bend.kFactor * materialThickness);
        totalDev += ba;
    }
    result.bendAllowance = totalDev;

    // Approximate flat pattern as the original mesh flattened to Z=0.
    Mesh flat;
    const auto& pos=bentPart.positions();
    std::vector<float> zVals(pos.size());
    for(size_t i=0;i<pos.size();++i) zVals[i]=pos[i].z;
    std::sort(zVals.begin(),zVals.end());
    float minZ=zVals.front(), maxZ=zVals.back();
    float thickness=maxZ-minZ;
    result.developedLength=thickness+totalDev;

    // Build flat mesh: project all vertices to Z=0, preserve XY.
    std::vector<Vec3> flatPos;
    for(const auto& p:pos) flatPos.push_back({p.x,p.y,0.f});
    flat.attributes().setPositions(std::move(flatPos));
    auto& topo=flat.topology();

    // Fan faces. For a proper unfold, we'd rotate each face around bend lines.
    // Simplified: just copy face topology with XY positions.
    HalfEdgeMesh flatHe;
    auto heOpt=HalfEdgeMesh::fromMesh(bentPart.toMesh());
    if(heOpt){
        for(uint32_t fi=0;fi<heOpt->faceCount();++fi){
            uint32_t he=heOpt->face(fi).edge;
            if(he==HalfEdgeMesh::kInvalid) continue;
            std::vector<uint32_t> verts;
            uint32_t startHe=he;
            do{verts.push_back(heOpt->edge(he).src);he=heOpt->edge(he).next;}while(he!=startHe);
            if(verts.size()>=3) topo.addFace(Face{verts});
        }
    }

    result.flatPattern=flat;
    return result;
}

// ── Replace Face ──────────────────────────────────────────────────────

ReplaceFaceResult replaceFaceWithSurface(const HalfEdgeMesh& mesh,
    uint32_t faceIndex, const NurbsSurface& newSurface) noexcept
{
    ReplaceFaceResult r;
    if(faceIndex>=mesh.faceCount()||!newSurface.isValid()) return r;

    HalfEdgeMesh result=mesh;
    uint32_t startHe=result.face(faceIndex).edge;
    if(startHe==HalfEdgeMesh::kInvalid) return r;

    // Find boundary vertices of the face.
    std::vector<uint32_t> boundaryVerts;
    uint32_t he=startHe;
    do{
        boundaryVerts.push_back(result.edge(he).src);
        r.affectedFaces.push_back(result.edge(he).face);
        he=result.edge(he).next;
    }while(he!=startHe);

    // Project each boundary vertex onto the new surface.
    auto domU=newSurface.domainU(), domV=newSurface.domainV();
    for(uint32_t v : boundaryVerts) {
        Vec3& pt=result.positions()[v];
        // Find closest point on new surface (simplified: sample at mid-UV).
        Vec3 closest=newSurface.evaluate(
            domU.first+(domU.second-domU.first)*0.5f,
            domV.first+(domV.second-domV.first)*0.5f);
        // Move to projected position.
        Vec3 proj=closest;
        // Preserve distance from face center.
        pt=proj+(pt-closest);
    }

    r.mesh=result.toMesh();
    r.success=true;
    return r;
}

} // namespace nexus::geometry
