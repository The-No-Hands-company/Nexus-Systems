#include "SoftwareRasterizer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace nexus::softrast {

using Vec3 = nexus::render::Vec3;
using Vec4 = nexus::render::Vec4;
using Mat4 = nexus::render::Mat4;

namespace {

inline Vec3 faceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
    const Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
    Vec3 n{ab.y * ac.z - ab.z * ac.y,
           ab.z * ac.x - ab.x * ac.z,
           ab.x * ac.y - ab.y * ac.x};
    const float l = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    return (l > 1e-20f) ? Vec3{n.x / l, n.y / l, n.z / l} : Vec3{0.f, 0.f, 1.f};
}

inline float dot3(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

} // namespace

void SoftwareRasterizer::renderImpl(PixelBuffer& buf, std::vector<float>& depth,
    std::vector<uint32_t>* idBuf, uint32_t objectId, const nexus::geometry::Mesh& mesh,
    const nexus::render::Camera& camera, const RasterizerConfig& cfg, const Mat4& model) const
{
    if (!mesh.attributes().hasPositions()) return;
    const auto& pos = mesh.attributes().positions();
    const size_t nVerts = pos.size();
    const auto& topo = mesh.topology();
    const size_t nFaces = topo.faceCount();
    const uint32_t W = buf.width(), H = buf.height();
    // `depth` is caller-owned (reversed-Z: near->1, far->0, so keep the LARGEST z).

    // ── Per-face geometric normals (world/model space) ───────────────────────
    std::vector<Vec3> faceN(nFaces, {0.f, 0.f, 1.f});
    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        if (!f.indicesInBounds(nVerts) || f.indices.size() < 3) continue;
        faceN[fi] = faceNormal(pos[f.indices[0]], pos[f.indices[1]], pos[f.indices[2]]);
    }

    // ── Shading normals ──────────────────────────────────────────────────────
    //  Authored normals (e.g. a smooth sphere) are respected directly. Otherwise
    //  auto-smooth per face-corner: blend only adjacent faces whose dihedral angle
    //  is below the smoothing threshold, so hard edges (cube, cylinder rim) stay
    //  faceted while curved surfaces read smooth — all without touching topology.
    const bool authored = mesh.attributes().hasNormals();
    std::vector<Vec3> vertN;                     // valid when authored
    std::vector<std::vector<Vec3>> cornerN;      // valid otherwise; [face][corner]
    if (authored) {
        vertN = mesh.attributes().normals();
    } else {
        std::vector<std::vector<uint32_t>> vertFaces(nVerts);
        for (size_t fi = 0; fi < nFaces; ++fi) {
            const auto& f = topo.face(fi);
            if (!f.indicesInBounds(nVerts) || f.indices.size() < 3) continue;
            for (uint32_t idx : f.indices) vertFaces[idx].push_back(static_cast<uint32_t>(fi));
        }
        const float cosThresh = std::cos(cfg.smoothingAngleDeg * 3.14159265f / 180.f);
        cornerN.resize(nFaces);
        for (size_t fi = 0; fi < nFaces; ++fi) {
            const auto& f = topo.face(fi);
            cornerN[fi].assign(f.indices.size(), faceN[fi]);
            if (!f.indicesInBounds(nVerts) || f.indices.size() < 3) continue;
            for (size_t c = 0; c < f.indices.size(); ++c) {
                Vec3 acc{0.f, 0.f, 0.f};
                for (uint32_t f2 : vertFaces[f.indices[c]]) {
                    if (dot3(faceN[fi], faceN[f2]) >= cosThresh) acc += faceN[f2];
                }
                const float l = std::sqrt(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);
                cornerN[fi][c] = (l > 1e-20f) ? Vec3{acc.x / l, acc.y / l, acc.z / l} : faceN[fi];
            }
        }
    }

    // ── Project vertices to clip space ───────────────────────────────────────
    const Mat4 mvp = camera.ubo().viewProj * model;
    auto mul = [](const Mat4& m, const Vec3& v) -> Vec4 {
        return {m.m[0][0]*v.x + m.m[0][1]*v.y + m.m[0][2]*v.z + m.m[0][3],
                m.m[1][0]*v.x + m.m[1][1]*v.y + m.m[1][2]*v.z + m.m[1][3],
                m.m[2][0]*v.x + m.m[2][1]*v.y + m.m[2][2]*v.z + m.m[2][3],
                m.m[3][0]*v.x + m.m[3][1]*v.y + m.m[3][2]*v.z + m.m[3][3]};
    };
    std::vector<Vec4> clip(nVerts);
    for (size_t i = 0; i < nVerts; ++i) clip[i] = mul(mvp, pos[i]);

    const float hw = W * 0.5f, hh = H * 0.5f;
    const bool wire = (cfg.mode == ShadingMode::Wireframe);
    const bool flat = (cfg.mode == ShadingMode::Flat);
    const float wireHalf = cfg.wireHalfWidthPx;

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        if (!f.indicesInBounds(nVerts) || f.indices.size() < 3) continue;
        const size_t n = f.indices.size();

        for (size_t k = 1; k + 1 < n; ++k) {
            const uint32_t i0 = f.indices[0], i1 = f.indices[k], i2 = f.indices[k + 1];
            const Vec4 v0 = clip[i0], v1 = clip[i1], v2 = clip[i2];
            if (v0.w < .001f || v1.w < .001f || v2.w < .001f) continue;
            const float iw0 = 1.f / v0.w, iw1 = 1.f / v1.w, iw2 = 1.f / v2.w;

            const float nx0 = v0.x*iw0, ny0 = v0.y*iw0, nz0 = v0.z*iw0;
            const float nx1 = v1.x*iw1, ny1 = v1.y*iw1, nz1 = v1.z*iw1;
            const float nx2 = v2.x*iw2, ny2 = v2.y*iw2, nz2 = v2.z*iw2;

            const float sx0 = (nx0+1.f)*hw, sy0 = (1.f-ny0)*hh;
            const float sx1 = (nx1+1.f)*hw, sy1 = (1.f-ny1)*hh;
            const float sx2 = (nx2+1.f)*hw, sy2 = (1.f-ny2)*hh;

            const int minX = std::max(0, (int)std::floor(std::min({sx0,sx1,sx2})));
            const int maxX = std::min((int)W-1, (int)std::ceil(std::max({sx0,sx1,sx2})));
            const int minY = std::max(0, (int)std::floor(std::min({sy0,sy1,sy2})));
            const int maxY = std::min((int)H-1, (int)std::ceil(std::max({sy0,sy1,sy2})));
            if (minX > maxX || minY > maxY) continue;

            const float area = (sx1-sx0)*(sy2-sy0) - (sx2-sx0)*(sy1-sy0);
            if (area == 0.f) continue;
            const float invA = 1.f / area;
            const float twoArea = std::abs(area);

            // Shading normal per triangle corner (0, k, k+1 of the polygon).
            Vec3 sn0, sn1, sn2;
            if (flat)          { sn0 = sn1 = sn2 = faceN[fi]; }
            else if (authored) { sn0 = vertN[i0]; sn1 = vertN[i1]; sn2 = vertN[i2]; }
            else               { sn0 = cornerN[fi][0]; sn1 = cornerN[fi][k]; sn2 = cornerN[fi][k+1]; }

            // Real polygon edges of this triangle (skip fan-triangulation diagonals):
            //  BC (corner k→k+1) is always a real edge; CA closes the polygon only on
            //  the last fan triangle; AB opens it only on the first.
            const bool edgeBC = true;
            const bool edgeCA = (k + 1 == n - 1);
            const bool edgeAB = (k == 1);
            const float lenBC = std::hypot(sx2 - sx1, sy2 - sy1);
            const float lenCA = std::hypot(sx0 - sx2, sy0 - sy2);
            const float lenAB = std::hypot(sx1 - sx0, sy1 - sy0);

            for (int py = minY; py <= maxY; ++py) {
                for (int px = minX; px <= maxX; ++px) {
                    const float cx = px + .5f, cy = py + .5f;
                    const float d0 = (sx1-sx2)*(cy-sy2) + (sx2-cx)*(sy1-sy2);
                    const float d1 = (sx2-sx0)*(cy-sy0) + (sx0-cx)*(sy2-sy0);
                    const float d2 = (sx0-sx1)*(cy-sy1) + (sx1-cx)*(sy0-sy1);
                    // Two-sided coverage: inside iff all edge functions share a sign
                    // (don't cull by winding — an inspection tool must show open
                    // surfaces like planes from both faces). Barycentric division by
                    // the signed area cancels the sign, so weights stay correct.
                    const bool inside = (d0>=0.f && d1>=0.f && d2>=0.f)
                                     || (d0<=0.f && d1<=0.f && d2<=0.f);
                    if (!inside) continue;
                    const float w0 = d0*invA, w1 = d1*invA, w2 = d2*invA;
                    const float z = w0*nz0 + w1*nz1 + w2*nz2;
                    const size_t idx = static_cast<size_t>(py)*W + px;
                    if (z <= depth[idx]) continue;   // reversed-Z: keep the nearest (largest z)
                    depth[idx] = z;   // written even for wireframe so faces occlude hidden edges
                    if (idBuf) (*idBuf)[idx] = objectId;   // front-most object per pixel

                    if (wire) {
                        // Screen-space distance to each edge: w_i * 2*area / edgeLength.
                        const float dBC = (lenBC > 1e-6f) ? std::abs(w0)*twoArea/lenBC : 1e30f;
                        const float dCA = (lenCA > 1e-6f) ? std::abs(w1)*twoArea/lenCA : 1e30f;
                        const float dAB = (lenAB > 1e-6f) ? std::abs(w2)*twoArea/lenAB : 1e30f;
                        const bool onEdge = (edgeBC && dBC < wireHalf)
                                         || (edgeCA && dCA < wireHalf)
                                         || (edgeAB && dAB < wireHalf);
                        if (!onEdge) continue;   // interior/diagonal: occlude but don't draw
                        buf.setPixel(px, py, cfg.wireColor);
                        continue;
                    }

                    Vec3 N{w0*sn0.x + w1*sn1.x + w2*sn2.x,
                           w0*sn0.y + w1*sn1.y + w2*sn2.y,
                           w0*sn0.z + w1*sn1.z + w2*sn2.z};
                    const float nl = std::sqrt(N.x*N.x + N.y*N.y + N.z*N.z);
                    if (nl > 1e-10f) { N.x/=nl; N.y/=nl; N.z/=nl; }
                    const float diff = std::max(0.f, dot3(N, cfg.lightDir));
                    const float l = cfg.ambientMin + (1.f - cfg.ambientMin)*diff;
                    const RGBA8 base = cfg.baseColor;
                    buf.setPixel(px, py, {
                        (uint8_t)std::min(255.f, l*base.r + .5f),
                        (uint8_t)std::min(255.f, l*base.g + .5f),
                        (uint8_t)std::min(255.f, l*base.b + .5f), 255});
                }
            }
        }
    }
}

} // namespace nexus::softrast
