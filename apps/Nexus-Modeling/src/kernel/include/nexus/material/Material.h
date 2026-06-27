#pragma once

#include <nexus/render/Camera.h> // Vec3, Vec4
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::material {

enum class TextureType : uint8_t { Albedo, Normal, Roughness, Metallic, AO, Height, Emission, Opacity, Count };

struct TextureRef {
    std::string path;
    TextureType type;
    float uvScale[2] = {1.f, 1.f};
    float uvOffset[2] = {0.f, 0.f};
};

struct PBRMaterial {
    std::string name;
    nexus::render::Vec4 albedo{0.8f, 0.8f, 0.8f, 1.f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ao = 1.0f;
    nexus::render::Vec3 emission{0.f, 0.f, 0.f};
    float emissionStrength = 0.f;
    float opacity = 1.f;
    std::vector<TextureRef> textures;

    bool hasTexture(TextureType t) const;
};

struct MaterialSlot {
    uint32_t materialIndex = 0;
    std::string targetMeshName;
    std::vector<uint32_t> targetFaceIndices;
};

class MaterialLibrary {
public:
    uint32_t addMaterial(const PBRMaterial& mat);
    const PBRMaterial& material(uint32_t index) const;
    PBRMaterial& material(uint32_t index);
    size_t materialCount() const;

    void addSlot(const MaterialSlot& slot);
    const std::vector<MaterialSlot>& slots() const;

    static PBRMaterial createDefault();
    static PBRMaterial createMetal(const std::string& name, float r, float g, float b, float roughness = 0.3f);
    static PBRMaterial createDielectric(const std::string& name, float r, float g, float b, float roughness = 0.5f);
    static PBRMaterial createEmissive(const std::string& name, float r, float g, float b, float strength = 1.0f);
    static std::string generateShaderDefines(const PBRMaterial& mat);
    static std::string generateShaderFunctions();

private:
    std::vector<PBRMaterial> m_materials;
    std::vector<MaterialSlot> m_slots;
};

} // namespace nexus::material
