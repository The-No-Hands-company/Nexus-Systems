#pragma once
#include "PixelBuffer.h"
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>
#include <cstdint>
#include <limits>
#include <vector>

namespace nexus::softrast {

enum class ShadingMode:uint8_t{Flat,Gouraud,Wireframe};

struct RasterizerConfig{
    ShadingMode mode=ShadingMode::Flat;
    RGBA8 background{30,30,30,255},baseColor{180,180,180,255},wireColor{220,220,220,255},specColor{255,255,255,255};
    nexus::render::Vec3 lightDir{.577f,.577f,.577f};
    float ambientMin=.15f,specStrength=0.f,shininess=32.f;
    // Smooth-shading (auto-smooth) dihedral threshold. When a mesh carries no
    // authored normals, adjacent faces whose angle is below this are blended
    // (smooth); sharper joins stay faceted. 60° keeps cubes/cylinder-rims hard
    // while smoothing spheres/cylinder-sides — the DCC "smoothing angle" model.
    float smoothingAngleDeg=60.f;
    // Wireframe line half-width in pixels.
    float wireHalfWidthPx=0.9f;
};

class SoftwareRasterizer{
public:
    // Single-object convenience: clears the buffer to background and owns its depth.
    void render(PixelBuffer&buf,const nexus::geometry::Mesh&mesh,const nexus::render::Camera&camera,const RasterizerConfig&cfg={})const{
        buf.clear(cfg.background);
        std::vector<float> depth(static_cast<size_t>(buf.width())*buf.height(),-std::numeric_limits<float>::infinity());
        renderImpl(buf,depth,nullptr,0u,mesh,camera,cfg,nexus::render::Mat4::identity());
    }
    void renderInto(PixelBuffer&buf,const nexus::geometry::Mesh&mesh,const nexus::render::Camera&camera,const RasterizerConfig&cfg,const nexus::render::Mat4&model)const{
        std::vector<float> depth(static_cast<size_t>(buf.width())*buf.height(),-std::numeric_limits<float>::infinity());
        renderImpl(buf,depth,nullptr,0u,mesh,camera,cfg,model);
    }
    // Scene path: the caller owns `depth` (init to -inf) and an optional object-ID
    // buffer (0 = empty). `objectId` is written into idBuf at every covered pixel that
    // wins the reversed-Z depth test, so the ID buffer holds the front-most object per
    // pixel — the seed for the selection outline. Does NOT clear the colour buffer.
    void renderShared(PixelBuffer&buf,std::vector<float>&depth,std::vector<uint32_t>*idBuf,
                      uint32_t objectId,const nexus::geometry::Mesh&mesh,const nexus::render::Camera&camera,
                      const RasterizerConfig&cfg,const nexus::render::Mat4&model)const{
        renderImpl(buf,depth,idBuf,objectId,mesh,camera,cfg,model);
    }
private:
    void renderImpl(PixelBuffer&buf,std::vector<float>&depth,std::vector<uint32_t>*idBuf,uint32_t objectId,
                    const nexus::geometry::Mesh&mesh,const nexus::render::Camera&camera,
                    const RasterizerConfig&cfg,const nexus::render::Mat4&model)const;
};

} // namespace nexus::softrast
