
#version 460
layout(push_constant) uniform PC {
    mat4 modelMatrix;
    vec4 albedo;
    float roughness;
    float metallic;
} pc;
layout(set = 0, binding = 0, std140) uniform Camera {
    mat4 view; mat4 projection; mat4 viewProj; mat4 invViewProj;
    vec4 position; vec4 direction; vec4 viewport;
    float fovY; float aspectRatio; float nearPlane; float farPlane;
    mat4 prevViewProj; vec4 jitter;
} cam;
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out float outRoughness;
layout(location = 4) out float outMetallic;
void main() {
    vec4 worldPos = pc.modelMatrix * vec4(inPos, 1.0);
    outWorldPos = worldPos.xyz;
    outNormal = mat3(pc.modelMatrix) * inNormal;
    outAlbedo = pc.albedo;
    outRoughness = pc.roughness;
    outMetallic = pc.metallic;
    gl_Position = cam.viewProj * worldPos;
}
