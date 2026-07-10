// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Visual Bridge — Pixel-to-Entity Resolution Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AgentVisualBridge.h>

#include <nexus/geometry/MeshBVH.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace nexus::agent {

AgentVisualBridge::AgentVisualBridge() = default;

void AgentVisualBridge::setCamera(const nexus::render::Camera& camera)
{
    m_camera = camera;
}

Vec3 AgentVisualBridge::screenToWorldRay(const VisualQuery& query,
                                          Vec3& origin, Vec3& direction) const
{
    Vec3 camPos = m_camera.position();
    const auto& ubo = m_camera.ubo();
    Vec3 forward{ubo.direction.x, ubo.direction.y, ubo.direction.z};
    Vec3 camUp{0, 1, 0};

    Vec3 right = forward.cross(camUp).normalize();
    Vec3 localUp = right.cross(forward).normalize();

    float w = std::max(query.screenWidth, 1.0f);
    float h = std::max(query.screenHeight, 1.0f);
    float aspect = w / h;

    constexpr float tanHalfFov = 0.41421356f;
    float nx = (2.0f * query.screenX / w - 1.0f) * aspect * tanHalfFov;
    float ny = (2.0f * (h - query.screenY) / h - 1.0f) * tanHalfFov;

    origin = camPos;
    Vec3 targetPoint = camPos + forward * 10.0f + right * nx + localUp * ny;
    direction = (targetPoint - camPos).normalize();
    return direction;
}

VisualHit AgentVisualBridge::rayCastFace(const Vec3& origin, const Vec3& direction,
                                           const nexus::geometry::Mesh& mesh) const
{
    VisualHit hit;
    hit.valid = false;

    const auto& positions = mesh.attributes().positions();
    const auto& topo = mesh.topology();

    float bestT = std::numeric_limits<float>::max();

    for (uint32_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);
        if (face.vertexCount() < 3) continue;

        for (size_t j = 0; j + 2 < face.vertexCount(); ++j) {
            uint32_t i0 = face.indices[0], i1 = face.indices[j + 1], i2 = face.indices[j + 2];
            if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) continue;

            const Vec3& v0 = positions[i0];
            const Vec3& v1 = positions[i1];
            const Vec3& v2 = positions[i2];

            Vec3 e1 = v1 - v0;
            Vec3 e2 = v2 - v0;
            Vec3 h = direction.cross(e2);
            float a = e1.dot(h);

            if (std::fabs(a) < 1e-7f) continue;

            float f = 1.0f / a;
            Vec3 s = origin - v0;
            float u = f * s.dot(h);

            if (u < 0.0f || u > 1.0f) continue;

            Vec3 q = s.cross(e1);
            float v = f * direction.dot(q);

            if (v < 0.0f || u + v > 1.0f) continue;

            float t = f * e2.dot(q);
            if (t < 1e-4f || t >= bestT) continue;

            bestT = t;
            hit.faceId = fi;
            hit.depth = t;
            hit.worldPosition = origin + direction * t;
            hit.worldNormal = e1.cross(e2).normalize();
            hit.valid = true;

            float bestDist = std::numeric_limits<float>::max();
            for (size_t k = 0; k < face.vertexCount(); ++k) {
                float d2 = (positions[face.indices[k]] - hit.worldPosition).lengthSq();
                if (d2 < bestDist) {
                    bestDist = d2;
                    hit.vertexId = face.indices[k];
                }
            }
        }
    }

    return hit;
}

VisualFeedback AgentVisualBridge::query(const VisualQuery& q,
                                         const nexus::geometry::Mesh& mesh) const
{
    VisualFeedback feedback;
    feedback.width = static_cast<uint32_t>(q.screenWidth);
    feedback.height = static_cast<uint32_t>(q.screenHeight);

    Vec3 origin, direction;
    (void)screenToWorldRay(q, origin, direction);

    auto hit = rayCastFace(origin, direction, mesh);
    if (hit.valid) {
        feedback.hits.push_back(hit);
        feedback.valid = true;
    }

    return feedback;
}

VisualFeedback AgentVisualBridge::queryRegion(const VisualRegion& region,
                                                const nexus::geometry::Mesh& mesh) const
{
    VisualFeedback feedback;

    float w = 1920.0f;
    float h = 1080.0f;

    VisualQuery centerQuery;
    centerQuery.screenX = (region.minX + region.maxX) * 0.5f * w;
    centerQuery.screenY = (region.minY + region.maxY) * 0.5f * h;
    centerQuery.screenWidth = w;
    centerQuery.screenHeight = h;

    auto centerFeedback = query(centerQuery, mesh);
    if (centerFeedback.valid) {
        feedback.hits = std::move(centerFeedback.hits);
        feedback.valid = true;
    }

    feedback.width = static_cast<uint32_t>(w);
    feedback.height = static_cast<uint32_t>(h);

    return feedback;
}

VisualFeedback AgentVisualBridge::queryByIdBuffer(
    const VisualQuery& q,
    const nexus::geometry::Mesh& mesh,
    const std::vector<uint32_t>& objectIdBuffer,
    uint32_t bufferWidth, uint32_t bufferHeight) const
{
    uint32_t px = static_cast<uint32_t>(q.screenX);
    uint32_t py = static_cast<uint32_t>(q.screenY);

    if (px < bufferWidth && py < bufferHeight) {
        uint32_t idx = py * bufferWidth + px;
        if (idx < objectIdBuffer.size()) {
            uint32_t entityId = objectIdBuffer[idx];
            if (entityId != ~0u) {
                VisualHit hit;
                hit.faceId = entityId;
                hit.valid = true;
                VisualFeedback feedback;
                feedback.hits.push_back(hit);
                feedback.valid = true;
                feedback.width = bufferWidth;
                feedback.height = bufferHeight;
                return feedback;
            }
        }
    }

    return query(q, mesh);
}

} // namespace nexus::agent
