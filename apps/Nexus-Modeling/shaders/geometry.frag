
#version 460
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inAlbedo;
layout(location = 3) in float inRoughness;
layout(location = 4) in float inMetallic;
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec2 outVelocity;
void main() {
    vec3 n = normalize(inNormal);
    outAlbedo = vec4(inAlbedo.rgb, inRoughness);
    outNormal = vec4(n * 0.5 + 0.5, inMetallic);
    outVelocity = vec2(0.0);
}
