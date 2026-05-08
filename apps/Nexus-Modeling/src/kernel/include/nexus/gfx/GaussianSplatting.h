#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — 3D Gaussian Splatting
//
//  Data model and scene integration for 3DGS representations.
//  Additive alongside the mesh pipeline; no IDevice/ICommandBuffer changes.
//  Public API: GaussianSplatCloud, GaussianSplatSceneNode, RepresentationType.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Camera.h>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <array>

namespace nexus::gfx {

// ── Per-primitive Gaussian splat ──────────────────────────────────────────────
struct GaussianSplat {
    nexus::render::Vec3 position   {};                     // world-space centroid
    nexus::render::Vec3 scale      {};                     // log-scale per-axis (exp gives radii)
    nexus::render::Vec4 rotation   {0.f, 0.f, 0.f, 1.f};  // unit quaternion xyzw
    float               opacity    = 0.f;                  // logit-space opacity (sigmoid → [0,1])
    std::array<float, 3>  dc       {};                     // degree-0 SH (base RGB)
    std::array<float, 45> shRest   {};                     // SH degree 1–3 coefficients (RGB × 15)
};

// ── Splat cloud (scene-level collection) ──────────────────────────────────────
struct GaussianSplatCloud {
    std::vector<GaussianSplat> splats;
    uint32_t                   shDegree     = 3;
    std::string                sourceFormat;   // "ply" | "splat" | ""

    // ── Serialisation ────────────────────────────────────────────────────────
    /// Load from a 3DGS-format binary PLY file on disk.
    [[nodiscard]] static std::optional<GaussianSplatCloud>
        loadFromPLY(const std::string& path);

    /// Load from a PLY byte buffer (enables in-memory and test use cases).
    [[nodiscard]] static std::optional<GaussianSplatCloud>
        loadFromPLYBytes(const std::vector<uint8_t>& bytes);

    /// Serialize to a binary_little_endian PLY file.
    [[nodiscard]] bool saveToPLY(const std::string& path) const;

    // ── Queries ───────────────────────────────────────────────────────────────
    [[nodiscard]] size_t splatCount() const noexcept { return splats.size(); }
    [[nodiscard]] bool   empty()      const noexcept { return splats.empty(); }
};

// ── Sort mode for alpha-correct rendering ─────────────────────────────────────
enum class SplatSortMode : uint8_t {
    None         = 0,   // unsorted (fast, incorrect blending)
    ViewDepthCPU = 1,   // back-to-front CPU sort (correct, moderate cost)
};

// ── Per-cloud render configuration ───────────────────────────────────────────
struct GaussianSplatRenderState {
    SplatSortMode sortMode              = SplatSortMode::ViewDepthCPU;
    float         splatScaleMultiplier  = 1.f;
    bool          useSphericalHarmonics = true;
    int           maxShDegree           = 3;    // clamped to GaussianSplatCloud::shDegree
};

// ── Scene node for Gaussian splat clouds ─────────────────────────────────────
// Parallel to render::Node (transform + visibility), carries a splat cloud.
// A scene can hold both Node (mesh) and GaussianSplatSceneNode (splats) without
// modifying existing IDevice or SceneGraph contracts.
struct GaussianSplatSceneNode {
    GaussianSplatCloud       cloud;
    nexus::render::Mat4      transform   {};
    GaussianSplatRenderState renderState {};
    bool                     visible     = true;

    GaussianSplatSceneNode() noexcept {
        transform = nexus::render::Mat4::identity();
    }

    explicit GaussianSplatSceneNode(GaussianSplatCloud c) noexcept
        : cloud(std::move(c))
    {
        transform = nexus::render::Mat4::identity();
    }
};

// ── Representation tag ────────────────────────────────────────────────────────
// Allows downstream systems to dispatch on representation type without
// dynamic_cast or void*.  New types can be added without touching existing cases.
enum class RepresentationType : uint8_t {
    Mesh          = 0,
    GaussianSplat = 1,
    PointCloud    = 2,
    Volume        = 3,
};

} // namespace nexus::gfx
