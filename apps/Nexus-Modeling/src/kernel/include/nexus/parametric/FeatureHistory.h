#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Parametric — Feature History DAG
//
//  Stores a graph of parametric features (extrude, revolve, future: fillet,
//  boolean, etc.) and auto-regenerates dirty features in topological order
//  when parameters change.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/parametric/ParametricFeature.h>
#include <nexus/parametric/ParametricSketchProfile.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nexus::parametric {

using FeatureId = uint32_t;
inline constexpr FeatureId kInvalidFeatureId = 0;

enum class FeatureKind : uint8_t {
    Sketch,
    Extrude,
    Revolve,
};

struct FeatureNode {
    FeatureId   id   = kInvalidFeatureId;
    FeatureKind kind = FeatureKind::Extrude;
    std::string name;

    // Parent feature (kInvalidFeatureId for root sketches).
    // Future: used by fillet/chamfer/boolean that consume parent mesh output.
    FeatureId   parent = kInvalidFeatureId;

    // The sketch that drives this feature.  Extrude/revolve consume it directly;
    // future operations (fillet/boolean) may not need a sketch.
    SketchProfile sketch;

    // Descriptors are stored here so the history can expose typed access.
    std::optional<geometry::CurveExtrudeDesc> extrudeDesc;
    std::optional<geometry::RevolveDesc>      revolveDesc;

    // Cached output.
    std::optional<geometry::Mesh>        mesh;
    std::optional<geometry::NurbsSurface> surf;

    // Material (simple PBR approximation).
    struct Material {
        float albedo[4] = {0.7f, 0.7f, 0.7f, 1.f}; // RGBA
        float roughness = 0.5f;
        float metallic  = 0.f;
    } material;

    // Primitive parameters (for parametric editing of placed primitives).
    enum class PrimType : uint8_t { None, Box, Sphere, Cylinder, Cone, Torus, Plane };
    PrimType primType = PrimType::None;
    float primParams[4] = {}; // dimensions: w/h/d, radius, etc.

    // Display mode (Solid, Wireframe, BoundingBox).
    enum class DisplayMode : uint8_t { Solid, Wireframe, BoundingBox };
    DisplayMode displayMode = DisplayMode::Solid;

    bool dirty = true;
    bool deleted = false;
    bool hidden = false;
};

struct FeatureHistoryReport {
    bool converged = true;
    std::vector<std::string> errors;
};

class FeatureHistory {
public:
    // Add a leaf feature from a sketch.
    [[nodiscard]] FeatureId addSketch(SketchProfile sketch);
    [[nodiscard]] FeatureId addExtrude(SketchProfile sketch,
                                        geometry::CurveExtrudeDesc desc = {});
    [[nodiscard]] FeatureId addRevolve(SketchProfile sketch,
                                        geometry::RevolveDesc desc = {});

    // ── Parameter setters (mark the node dirty) ────────────────────
    bool setDirection(FeatureId id, nexus::render::Vec3 direction);
    bool setHeight(FeatureId id, float h);
    bool setDraftAngle(FeatureId id, float deg);
    bool setHeightSamples(FeatureId id, uint32_t n);
    bool setCapEnds(FeatureId id, bool enabled);

    bool setAxis(FeatureId id, nexus::render::Vec3 origin, nexus::render::Vec3 direction);
    bool setStartAngle(FeatureId id, float deg);
    bool setEndAngle(FeatureId id, float deg);
    bool setAngularSamples(FeatureId id, uint32_t n);

    // ── Name ───────────────────────────────────────────────────────
    bool setName(FeatureId id, const std::string& name);

    // ── Delete ─────────────────────────────────────────────────────
    bool removeFeature(FeatureId id);
    bool setHidden(FeatureId id, bool hidden);
    bool unhideAll();

    // ── Rebuild ────────────────────────────────────────────────────
    // Regenerate all dirty features.  Returns a report with errors.
    FeatureHistoryReport rebuild();

    // ── Query ──────────────────────────────────────────────────────
    [[nodiscard]] size_t    featureCount() const noexcept { return m_nodes.size(); }
    [[nodiscard]] bool      isDirty(FeatureId id) const noexcept;
    [[nodiscard]] FeatureId parent(FeatureId id) const noexcept;
    [[nodiscard]] FeatureKind kind(FeatureId id) const noexcept;

    // ── Typed access to nodes ──────────────────────────────────────
    [[nodiscard]] FeatureNode*       node(FeatureId id) noexcept;
    [[nodiscard]] const FeatureNode* node(FeatureId id) const noexcept;

    // Convenience: access the node's mesh or surface directly.
    [[nodiscard]] const geometry::Mesh*        mesh(FeatureId id) const noexcept;
    [[nodiscard]] const geometry::NurbsSurface* surface(FeatureId id) const noexcept;

private:
    [[nodiscard]] FeatureNode* findNode(FeatureId id) noexcept;
    [[nodiscard]] const FeatureNode* findNode(FeatureId id) const noexcept;

    std::vector<FeatureNode> m_nodes;
    FeatureId                m_nextId = 1;
};

} // namespace nexus::parametric
