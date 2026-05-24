#version 460
#extension GL_EXT_ray_tracing : require
// ─────────────────────────────────────────────────────────────────────────────
//  Bring-up closest-hit shader: returns the barycentric coordinates of the hit
//  as a debug color (red/green/blue toward each triangle vertex).
//  Kept in sync with tests/kernel/test_VulkanRayTracingDispatch.cpp.
// ─────────────────────────────────────────────────────────────────────────────

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;

void main()
{
    const vec3 barycentric = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    hitValue = barycentric;
}
