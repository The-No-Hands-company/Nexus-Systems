#include <nexus/sculpt/Brush.h>

#include <algorithm>
#include <cmath>

namespace nexus::sculpt {

float evaluateFalloff(FalloffShape shape, float t01) noexcept {
    float t = std::clamp(t01, 0.0f, 1.0f);

    switch (shape) {
    case FalloffShape::Constant:
        return 1.0f;

    case FalloffShape::Linear:
        return 1.0f - t;

    case FalloffShape::Smooth:
        return 1.0f - (3.0f * t * t - 2.0f * t * t * t);

    case FalloffShape::Sharp:
        return (1.0f - t) * (1.0f - t);

    case FalloffShape::Root:
        return std::sqrt(1.0f - t);

    case FalloffShape::Sphere:
        return std::sqrt(1.0f - t * t);

    case FalloffShape::InverseSmooth: {
        float u = 1.0f - t;
        return u * u * (3.0f - 2.0f * u);
    }

    case FalloffShape::BellCurve: {
        float mid = 0.5f;
        float sigma = 0.2f;
        return std::exp(-(t - mid) * (t - mid) / (2.0f * sigma * sigma));
    }

    case FalloffShape::Custom:
        return 1.0f - t;

    default:
        return 1.0f - t;
    }
}


float evaluatePressureCurve(float pressure, float hardness, float minRadius, float maxRadius) noexcept {
    pressure = std::clamp(pressure, 0.0f, 1.0f);

    if (hardness <= 0.0f) return maxRadius;
    if (hardness >= 1.0f) return minRadius;

    float exponent = 1.0f / std::max(hardness, 0.01f);
    float curved = std::pow(pressure, exponent);

    return minRadius + (maxRadius - minRadius) * curved;
}

float evaluateBrushStrength(float pressure, float hardness, float baseStrength) noexcept {
    pressure = std::clamp(pressure, 0.0f, 1.0f);

    if (hardness <= 0.0f) return baseStrength;

    float curvedPressure = std::pow(pressure, std::max(hardness, 0.01f));
    return baseStrength * curvedPressure;
}

} // namespace nexus::sculpt
