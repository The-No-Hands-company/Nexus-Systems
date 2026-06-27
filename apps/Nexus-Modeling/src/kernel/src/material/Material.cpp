#include "nexus/material/Material.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <numbers>

namespace nexus::material {

bool PBRMaterial::hasTexture(TextureType t) const {
    return std::any_of(textures.begin(), textures.end(),
                        [t](const TextureRef& ref) { return ref.type == t; });
}

uint32_t MaterialLibrary::addMaterial(const PBRMaterial& mat) {
    const uint32_t index = static_cast<uint32_t>(m_materials.size());
    m_materials.push_back(mat);
    return index;
}

const PBRMaterial& MaterialLibrary::material(uint32_t index) const {
    return m_materials[index];
}

PBRMaterial& MaterialLibrary::material(uint32_t index) {
    return m_materials[index];
}

size_t MaterialLibrary::materialCount() const {
    return m_materials.size();
}

void MaterialLibrary::addSlot(const MaterialSlot& slot) {
    m_slots.push_back(slot);
}

const std::vector<MaterialSlot>& MaterialLibrary::slots() const {
    return m_slots;
}

PBRMaterial MaterialLibrary::createDefault() {
    PBRMaterial mat;
    mat.name = "Default";
    mat.albedo = {0.8f, 0.8f, 0.8f, 1.0f};
    mat.roughness = 0.5f;
    mat.metallic = 0.0f;
    mat.ao = 1.0f;
    return mat;
}

PBRMaterial MaterialLibrary::createMetal(const std::string& name, float r, float g, float b, float roughness) {
    PBRMaterial mat;
    mat.name = name;
    mat.albedo = {r, g, b, 1.0f};
    mat.roughness = roughness;
    mat.metallic = 1.0f;
    mat.ao = 1.0f;
    return mat;
}

PBRMaterial MaterialLibrary::createDielectric(const std::string& name, float r, float g, float b, float roughness) {
    PBRMaterial mat;
    mat.name = name;
    mat.albedo = {r, g, b, 1.0f};
    mat.roughness = roughness;
    mat.metallic = 0.0f;
    mat.ao = 1.0f;
    return mat;
}

PBRMaterial MaterialLibrary::createEmissive(const std::string& name, float r, float g, float b, float strength) {
    PBRMaterial mat;
    mat.name = name;
    mat.albedo = {0.0f, 0.0f, 0.0f, 1.0f};
    mat.roughness = 1.0f;
    mat.metallic = 0.0f;
    mat.emission = {r, g, b};
    mat.emissionStrength = strength;
    mat.ao = 1.0f;
    return mat;
}

std::string MaterialLibrary::generateShaderDefines(const PBRMaterial& mat) {
    std::ostringstream oss;

    oss << "#define MATERIAL_ALBEDO vec4("
        << mat.albedo.x << "," << mat.albedo.y << "," << mat.albedo.z << "," << mat.albedo.w << ")\n";
    oss << "#define MATERIAL_ROUGHNESS " << mat.roughness << "\n";
    oss << "#define MATERIAL_METALLIC " << mat.metallic << "\n";
    oss << "#define MATERIAL_AO " << mat.ao << "\n";
    oss << "#define MATERIAL_EMISSION vec3("
        << mat.emission.x << "," << mat.emission.y << "," << mat.emission.z << ")\n";
    oss << "#define MATERIAL_EMISSION_STRENGTH " << mat.emissionStrength << "\n";
    oss << "#define MATERIAL_OPACITY " << mat.opacity << "\n";

    if (mat.hasTexture(TextureType::Albedo)) oss << "#define HAS_ALBEDO_TEXTURE 1\n";
    if (mat.hasTexture(TextureType::Normal)) oss << "#define HAS_NORMAL_TEXTURE 1\n";
    if (mat.hasTexture(TextureType::Roughness)) oss << "#define HAS_ROUGHNESS_TEXTURE 1\n";
    if (mat.hasTexture(TextureType::Metallic)) oss << "#define HAS_METALLIC_TEXTURE 1\n";
    if (mat.hasTexture(TextureType::AO)) oss << "#define HAS_AO_TEXTURE 1\n";
    if (mat.hasTexture(TextureType::Height)) oss << "#define HAS_HEIGHT_TEXTURE 1\n";
    if (mat.hasTexture(TextureType::Emission)) oss << "#define HAS_EMISSION_TEXTURE 1\n";
    if (mat.hasTexture(TextureType::Opacity)) oss << "#define HAS_OPACITY_TEXTURE 1\n";

    return oss.str();
}

std::string MaterialLibrary::generateShaderFunctions() {
    std::ostringstream oss;

    oss << R"(
// PBR Shading Functions
const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 computePBR(vec3 N, vec3 V, vec3 L, vec3 albedo, float roughness, float metallic, vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 specular = (F * D * G) / max(4.0 * NdotL * NdotV, 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    return (kD * albedo / PI + specular) * NdotL;
}
)";

    return oss.str();
}

} // namespace nexus::material
