
#version 460
layout(set = 0, binding = 0) uniform texture2D uAlbedo;
layout(set = 0, binding = 1) uniform texture2D uNormal;
layout(set = 0, binding = 2) uniform texture2D uVelocity;
layout(set = 0, binding = 3) uniform texture2D uDepth;
layout(set = 0, binding = 4) uniform sampler uAlbedoSampler;
layout(set = 0, binding = 5) uniform sampler uNormalSampler;
layout(set = 0, binding = 6) uniform sampler uVelocitySampler;
layout(set = 0, binding = 7) uniform sampler uDepthSampler;
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ggxDistribution(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float smithGeometry(float NdotV, float NdotL, float roughness) {
    float a = roughness * roughness;
    float k = a * 0.5;
    float g1 = NdotV / (NdotV * (1.0 - k) + k);
    float g2 = NdotL / (NdotL * (1.0 - k) + k);
    return g1 * g2;
}

void main() {
    vec4  albedoPacked  = texture(sampler2D(uAlbedo, uAlbedoSampler), vUv);
    vec4  normalPacked  = texture(sampler2D(uNormal, uNormalSampler), vUv);
    vec3  baseColor     = albedoPacked.rgb;
    float roughness     = albedoPacked.a;
    float metallic      = normalPacked.a;
    vec3  n             = normalize(normalPacked.xyz * 2.0 - 1.0);

    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 viewDir  = normalize(vec3(0.0, 0.0, 1.0));
    vec3 halfVec  = normalize(lightDir + viewDir);

    float NdotL = max(dot(n, lightDir), 0.0);
    float NdotV = max(dot(n, viewDir), 0.0);
    float NdotH = max(dot(n, halfVec), 0.0);
    float HdotV = max(dot(halfVec, viewDir), 0.0);

    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 F  = fresnelSchlick(HdotV, F0);
    float D = ggxDistribution(NdotH, roughness);
    float G = smithGeometry(NdotV, NdotL, roughness);

    vec3 specular = (D * F * G) / max(4.0 * NdotV * NdotL + 0.0001, 0.0001);
    vec3 diffuse  = (1.0 - F) * (1.0 - metallic) * baseColor / PI;

    float ambient = 0.03;
    vec3 color = baseColor * ambient + (diffuse + specular) * NdotL;

    // Second light from opposite direction for fill.
    vec3 lightDir2 = normalize(vec3(-0.3, 0.5, -0.2));
    float NdotL2 = max(dot(n, lightDir2), 0.0);
    color += baseColor * 0.15 * NdotL2;

    // Subtle rim light.
    float rim = 1.0 - abs(dot(n, viewDir));
    color += vec3(rim * rim * 0.1);

    outColor = vec4(color, 1.0);
}
