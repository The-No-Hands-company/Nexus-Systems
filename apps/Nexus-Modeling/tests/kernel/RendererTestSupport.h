#pragma once

#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>

namespace nexus::render::testsupport {

inline Mat4 makeDeterministicCascadeLightViewProj(float worldXCenter)
{
    Mat4 result = Mat4::identity();
    result.m[0][0] = 1.0f;
    result.m[0][3] = -worldXCenter;
    result.m[2][0] = 0.0f;
    result.m[2][1] = 0.0f;
    result.m[2][2] = 0.0f;
    result.m[2][3] = 0.5f;
    return result;
}

inline Node* addDeterministicShadowCaster(SceneGraph& scene,
                                          const char* name,
                                          uint64_t vertexBufferId,
                                          uint64_t indexBufferId,
                                          const Vec3& worldPosition,
                                          bool castShadow = true)
{
    Node* node = scene.createNode(name);
    if (node == nullptr) {
        return nullptr;
    }

    node->mesh.vertexBuffer.id = vertexBufferId;
    node->mesh.indexBuffer.id = indexBufferId;
    node->mesh.indexCount = 3;
    node->castShadow = castShadow;
    node->localTransform().translation = worldPosition;
    return node;
}

} // namespace nexus::render::testsupport
