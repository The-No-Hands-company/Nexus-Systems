#include <nexus/geometry/TransformOrientation.h>

#include <cmath>

namespace nexus::geometry {
using Vec3 = nexus::render::Vec3;

TransformFrame TransformOrientation::fromFace(
    const Vec3& faceNormal, const Vec3& centroid) {
    TransformFrame o;
    o.origin = centroid;
    o.axisZ = faceNormal;

    float nz = std::sqrt(faceNormal.x * faceNormal.x + faceNormal.y * faceNormal.y + faceNormal.z * faceNormal.z);
    if (nz > 1e-10f) {
        o.axisZ = {faceNormal.x / nz, faceNormal.y / nz, faceNormal.z / nz};
    }

    Vec3 worldUp{0, 1, 0};
    if (std::abs(o.axisZ.y) > 0.99f) {
        worldUp = {1, 0, 0};
    }
    o.axisX = {worldUp.y * o.axisZ.z - worldUp.z * o.axisZ.y,
               worldUp.z * o.axisZ.x - worldUp.x * o.axisZ.z,
               worldUp.x * o.axisZ.y - worldUp.y * o.axisZ.x};
    float nx = std::sqrt(o.axisX.x * o.axisX.x + o.axisX.y * o.axisX.y + o.axisX.z * o.axisX.z);
    if (nx > 1e-10f) {
        o.axisX = {o.axisX.x / nx, o.axisX.y / nx, o.axisX.z / nx};
    }
    o.axisY = {o.axisZ.y * o.axisX.z - o.axisZ.z * o.axisX.y,
               o.axisZ.z * o.axisX.x - o.axisZ.x * o.axisX.z,
               o.axisZ.x * o.axisX.y - o.axisZ.y * o.axisX.x};
    return o;
}

TransformFrame TransformOrientation::fromEdge(
    const Vec3& edgeStart, const Vec3& edgeEnd) {
    TransformFrame o;
    o.origin = edgeStart;

    Vec3 dir{edgeEnd.x - edgeStart.x, edgeEnd.y - edgeStart.y, edgeEnd.z - edgeStart.z};
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 1e-10f) {
        o.axisX = {dir.x / len, dir.y / len, dir.z / len};
    } else {
        o.axisX = {1, 0, 0};
    }

    Vec3 worldUp{0, 1, 0};
    if (std::abs(o.axisX.y) > 0.99f) {
        worldUp = {0, 0, 1};
    }

    o.axisZ = {o.axisX.y * worldUp.z - o.axisX.z * worldUp.y,
               o.axisX.z * worldUp.x - o.axisX.x * worldUp.z,
               o.axisX.x * worldUp.y - o.axisX.y * worldUp.x};
    float nz = std::sqrt(o.axisZ.x * o.axisZ.x + o.axisZ.y * o.axisZ.y + o.axisZ.z * o.axisZ.z);
    if (nz > 1e-10f) {
        o.axisZ = {o.axisZ.x / nz, o.axisZ.y / nz, o.axisZ.z / nz};
    }

    o.axisY = {o.axisZ.y * o.axisX.z - o.axisZ.z * o.axisX.y,
               o.axisZ.z * o.axisX.x - o.axisZ.x * o.axisX.z,
               o.axisZ.x * o.axisX.y - o.axisZ.y * o.axisX.x};
    return o;
}

TransformFrame TransformOrientation::fromCursor(const Vec3& position) {
    TransformFrame o;
    o.origin = position;
    o.axisX = {1, 0, 0};
    o.axisY = {0, 1, 0};
    o.axisZ = {0, 0, 1};
    return o;
}

TransformFrame TransformOrientation::fromView(
    const Vec3& cameraPosition, const Vec3& target, const Vec3& up) {
    TransformFrame o;
    o.origin = target;

    Vec3 forward{target.x - cameraPosition.x, target.y - cameraPosition.y, target.z - cameraPosition.z};
    float fl = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (fl > 1e-10f) {
        o.axisZ = {forward.x / fl, forward.y / fl, forward.z / fl};
    } else {
        o.axisZ = {0, 0, 1};
    }

    Vec3 right{o.axisZ.y * up.z - o.axisZ.z * up.y,
               o.axisZ.z * up.x - o.axisZ.x * up.z,
               o.axisZ.x * up.y - o.axisZ.y * up.x};
    float rl = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (rl > 1e-10f) {
        o.axisX = {right.x / rl, right.y / rl, right.z / rl};
    }
    o.axisY = {o.axisZ.y * o.axisX.z - o.axisZ.z * o.axisX.y,
               o.axisZ.z * o.axisX.x - o.axisZ.x * o.axisX.z,
               o.axisZ.x * o.axisX.y - o.axisZ.y * o.axisX.x};
    return o;
}

TransformFrame TransformOrientation::fromGlobal(const Vec3& origin) {
    TransformFrame o;
    o.origin = origin;
    o.axisX = {1, 0, 0};
    o.axisY = {0, 1, 0};
    o.axisZ = {0, 0, 1};
    return o;
}

TransformFrame TransformOrientation::fromLocal(
    const TransformFrame& existing, const Vec3& newOrigin) {
    TransformFrame o = existing;
    o.origin = newOrigin;
    return o;
}

bool TransformOrientation::isOrthonormal(
    const TransformFrame& o, float epsilon) {
    float lenX = std::sqrt(o.axisX.x * o.axisX.x + o.axisX.y * o.axisX.y + o.axisX.z * o.axisX.z);
    float lenY = std::sqrt(o.axisY.x * o.axisY.y + o.axisY.y * o.axisY.y + o.axisY.z * o.axisY.z);
    float lenZ = std::sqrt(o.axisZ.x * o.axisZ.x + o.axisZ.y * o.axisZ.y + o.axisZ.z * o.axisZ.z);
    if (std::abs(lenX - 1.0f) > epsilon || std::abs(lenY - 1.0f) > epsilon || std::abs(lenZ - 1.0f) > epsilon) return false;

    float dotXY = o.axisX.x * o.axisY.x + o.axisX.y * o.axisY.y + o.axisX.z * o.axisY.z;
    float dotXZ = o.axisX.x * o.axisZ.x + o.axisX.y * o.axisZ.y + o.axisX.z * o.axisZ.z;
    float dotYZ = o.axisY.x * o.axisZ.x + o.axisY.y * o.axisZ.y + o.axisY.z * o.axisZ.z;
    return std::abs(dotXY) < epsilon && std::abs(dotXZ) < epsilon && std::abs(dotYZ) < epsilon;
}

} // namespace nexus::geometry
