#version 460
#extension GL_EXT_ray_tracing : require
// ─────────────────────────────────────────────────────────────────────────────
//  Bring-up miss shader: rays that hit nothing return the background color.
//  Kept in sync with tests/kernel/test_VulkanRayTracingDispatch.cpp.
// ─────────────────────────────────────────────────────────────────────────────

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    hitValue = vec3(0.05, 0.05, 0.08); // matches the deferred composite clear color
}
