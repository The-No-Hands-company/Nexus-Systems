#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Transform Gizmo
//
//  Interactive translate/rotate/scale on selected objects using arrow handles.
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/render/Camera.h>

namespace nexus::app {

using Vec3 = nexus::render::Vec3;

class TransformGizmo {
public:
    enum class Axis { None, X, Y, Z };
    enum class Mode { Translate, Rotate, Scale };

    // Render the gizmo at the given position.
    void render(const Vec3& center) const noexcept;

    // Check if a mouse ray hits a gizmo axis handle.
    Axis pickAxis(const Vec3& center, const Vec3& rayOrigin,
                  const Vec3& rayDirection) const noexcept;

    // Apply translation to a feature mesh along an axis.
    void translate(const Vec3& center, Axis axis, float amount,
                   nexus::cad::CadDocument& doc,
                   nexus::parametric::FeatureId fid,
                   bool pushUndo = true) const noexcept;

    // Apply uniform/non-uniform scale around the center.
    void scale(const Vec3& center, Axis axis, float factor,
               nexus::cad::CadDocument& doc,
               nexus::parametric::FeatureId fid,
               bool pushUndo = true) const noexcept;

    // Apply rotation around an axis through the center.
    void rotate(const Vec3& center, Axis axis, float angleRad,
                nexus::cad::CadDocument& doc,
                nexus::parametric::FeatureId fid,
                bool pushUndo = true) const noexcept;

    void setMode(Mode m) { m_mode = m; }
    [[nodiscard]] Mode mode() const noexcept { return m_mode; }

private:
    Mode m_mode = Mode::Translate;
    float m_handleLength = 1.5f;
    float m_handleRadius = 0.08f;
};

} // namespace nexus::app
