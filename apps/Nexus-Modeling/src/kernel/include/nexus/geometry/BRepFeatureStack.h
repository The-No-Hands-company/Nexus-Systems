#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — non-destructive analytic B-rep feature stack
//
//  A parametric modelling history: an ordered list of Features whose first entry
//  is a BASE primitive (box / cylinder / sphere / extrude / revolve) and whose
//  rest are MODIFIERS (transform / chamfer / fillet) applied in turn. evaluate()
//  replays the whole stack from scratch, so the base is never mutated — editing
//  any feature's parameters and re-evaluating cascades the change through the
//  chain (the "modifier stack" of Blender / 3ds Max, for the analytic B-rep).
//
//  Robustness: a modifier that fails (returns an empty/invalid Body) is skipped,
//  leaving the running solid unchanged, and evaluate() never returns a Body that
//  fails checkIntegrity — it returns a valid solid or a clean empty Body.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry::brep {

enum class FeatureKind : std::uint8_t {
    // Bases (produce the initial Body).
    Box,
    FacetedCylinder,
    FacetedSphere,
    Extrude,
    Revolve,
    // Modifiers (Body → Body).
    Transform,
    Chamfer,
    Fillet,
};

// One parametric operation. Only the fields relevant to `kind` are used; edit
// them and re-evaluate the owning stack to update the model.
struct Feature {
    FeatureKind kind = FeatureKind::Box;

    // Base dimensions: box w/h/d; cylinder & sphere radius in `w`, height in `h`.
    float w = 1.f, h = 1.f, d = 1.f;
    std::uint32_t segments = 8, lat = 8, lon = 12;
    // Extrude / Revolve geometry.
    std::vector<Vec3> profile;
    Vec3 dir{0.f, 0.f, 1.f}, axisOrigin{0.f, 0.f, 0.f}, axisDir{0.f, 0.f, 1.f};

    // Modifier parameters.
    nexus::render::Mat4 xform = nexus::render::Mat4::identity();  // Transform
    float amount = 0.f;                                           // Chamfer setback / Fillet radius
    int axis = 0, s1 = 1, s2 = 1;                                 // Chamfer / Fillet edge selector

    [[nodiscard]] bool isBase() const noexcept
    {
        return kind == FeatureKind::Box || kind == FeatureKind::FacetedCylinder ||
               kind == FeatureKind::FacetedSphere || kind == FeatureKind::Extrude ||
               kind == FeatureKind::Revolve;
    }

    [[nodiscard]] static Feature box(float w, float h, float d);
    [[nodiscard]] static Feature facetedCylinder(float radius, float height, std::uint32_t segments);
    [[nodiscard]] static Feature facetedSphere(float radius, std::uint32_t lat, std::uint32_t lon);
    [[nodiscard]] static Feature extrude(std::vector<Vec3> profile, Vec3 dir);
    [[nodiscard]] static Feature revolve(std::vector<Vec3> profile, Vec3 axisOrigin, Vec3 axisDir,
                                         std::uint32_t segments);
    [[nodiscard]] static Feature transform(const nexus::render::Mat4& m);
    [[nodiscard]] static Feature chamfer(int axis, int s1, int s2, float setback);
    [[nodiscard]] static Feature fillet(int axis, int s1, int s2, float radius,
                                        std::uint32_t segments);
};

class FeatureStack {
public:
    void add(Feature f);
    void insert(std::size_t index, Feature f);
    void remove(std::size_t index);
    [[nodiscard]] std::size_t size() const noexcept { return m_features.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_features.empty(); }
    [[nodiscard]] Feature& at(std::size_t i) { return m_features[i]; }
    [[nodiscard]] const Feature& at(std::size_t i) const { return m_features[i]; }

    // Replay the whole stack and return the resulting solid. Pure (const): the
    // base and every feature are untouched. Returns an empty Body if the stack is
    // empty or its first feature is not a base.
    [[nodiscard]] Body evaluate() const;

    // Versioned binary serialization of the parametric history (every Feature's
    // kind + parameters + profile + transform), so an EDITABLE model can be saved
    // and reopened — not just the baked solid. Deterministic. deserialize returns
    // nullopt on a bad magic/version, a truncated/garbage buffer, or a non-finite
    // float in the stream (bit-inspection checked).
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;
    [[nodiscard]] static std::optional<FeatureStack> deserialize(
        const std::vector<std::uint8_t>& bytes);

private:
    std::vector<Feature> m_features;
};

}  // namespace nexus::geometry::brep
