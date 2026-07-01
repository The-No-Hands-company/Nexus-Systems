#pragma once
// ─── Nexus Geometry ── TransformOrientation ────────────────────────────────
//  Custom transform orientations: face normal, edge alignment,
//  custom cursor, local axes from selections.

#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct TransformFrame {
    Vec3 origin;
    Vec3 axisX;
    Vec3 axisY;
    Vec3 axisZ;
};

enum class OrientationMode : uint8_t {
    Global,
    Local,
    Normal,
    Gimbal,
    View,
    Cursor,
    Edge,
    Face,
};

class TransformOrientation {
public:
    using Vec3 = nexus::render::Vec3;

    [[nodiscard]] static TransformFrame fromFace(
        const Vec3& faceNormal, const Vec3& centroid);

    [[nodiscard]] static TransformFrame fromEdge(
        const Vec3& edgeStart, const Vec3& edgeEnd);

    [[nodiscard]] static TransformFrame fromCursor(
        const Vec3& position);

    [[nodiscard]] static TransformFrame fromView(
        const Vec3& cameraPosition, const Vec3& target,
        const Vec3& up);

    [[nodiscard]] static TransformFrame fromGlobal(const Vec3& origin);

    [[nodiscard]] static TransformFrame fromLocal(
        const TransformFrame& existing, const Vec3& newOrigin);

    [[nodiscard]] static bool isOrthonormal(
        const TransformFrame& o, float epsilon = 1e-6f);
};

} // namespace nexus::geometry
