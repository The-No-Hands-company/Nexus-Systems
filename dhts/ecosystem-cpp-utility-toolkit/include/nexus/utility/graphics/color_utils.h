#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <string>
#include <array>

namespace nexus::utility::graphics {

/**
 * @brief Color utilities for RGB/HSV/HSL conversions and operations.
 */
struct Color {
    float r, g, b, a;
    
    constexpr Color(float r = 0, float g = 0, float b = 0, float a = 1.0f)
        : r(r), g(g), b(b), a(a) {}
    
    static constexpr Color fromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }
    
    uint32_t toRGBA32() const {
        return (static_cast<uint32_t>(r * 255) << 24) |
               (static_cast<uint32_t>(g * 255) << 16) |
               (static_cast<uint32_t>(b * 255) << 8) |
               static_cast<uint32_t>(a * 255);
    }
};

class ColorUtils {
public:
    /**
     * @brief Converts RGB to HSV.
     */
    static std::array<float, 3> rgbToHSV(float r, float g, float b) {
        float max = std::max({r, g, b});
        float min = std::min({r, g, b});
        float delta = max - min;
        
        float h = 0, s = 0, v = max;
        
        if (delta > 0.00001f) {
            s = delta / max;
            
            if (max == r) {
                h = 60.0f * fmod((g - b) / delta, 6.0f);
            } else if (max == g) {
                h = 60.0f * ((b - r) / delta + 2.0f);
            } else {
                h = 60.0f * ((r - g) / delta + 4.0f);
            }
            
            if (h < 0) h += 360.0f;
        }
        
        return {h, s, v};
    }
    
    /**
     * @brief Converts HSV to RGB.
     */
    static std::array<float, 3> hsvToRGB(float h, float s, float v) {
        float c = v * s;
        float x = c * (1.0f - std::abs(fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        
        float r = 0, g = 0, b = 0;
        
        if (h < 60) {
            r = c; g = x; b = 0;
        } else if (h < 120) {
            r = x; g = c; b = 0;
        } else if (h < 180) {
            r = 0; g = c; b = x;
        } else if (h < 240) {
            r = 0; g = x; b = c;
        } else if (h < 300) {
            r = x; g = 0; b = c;
        } else {
            r = c; g = 0; b = x;
        }
        
        return {r + m, g + m, b + m};
    }
    
    /**
     * @brief Linear interpolation between colors.
     */
    static Color lerp(const Color& a, const Color& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return Color(
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t
        );
    }
    
    /**
     * @brief Alpha blending.
     */
    static Color blend(const Color& src, const Color& dst) {
        float out_a = src.a + dst.a * (1.0f - src.a);
        if (out_a < 0.00001f) return Color(0, 0, 0, 0);
        
        return Color(
            (src.r * src.a + dst.r * dst.a * (1.0f - src.a)) / out_a,
            (src.g * src.a + dst.g * dst.a * (1.0f - src.a)) / out_a,
            (src.b * src.a + dst.b * dst.a * (1.0f - src.a)) / out_a,
            out_a
        );
    }
    
    /**
     * @brief Additive blending.
     */
    static Color additive(const Color& a, const Color& b) {
        return Color(
            std::min(a.r + b.r, 1.0f),
            std::min(a.g + b.g, 1.0f),
            std::min(a.b + b.b, 1.0f),
            std::min(a.a + b.a, 1.0f)
        );
    }
    
    /**
     * @brief Multiply blending.
     */
    static Color multiply(const Color& a, const Color& b) {
        return Color(a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a);
    }
    
    /**
     * @brief Parses hex color string (#RRGGBB or #RRGGBBAA).
     */
    static Color fromHex(const std::string& hex) {
        if (hex.empty() || hex[0] != '#') return Color();
        
        uint32_t value = std::stoul(hex.substr(1), nullptr, 16);
        
        if (hex.length() == 7) { // #RRGGBB
            return Color::fromRGBA(
                (value >> 16) & 0xFF,
                (value >> 8) & 0xFF,
                value & 0xFF
            );
        } else if (hex.length() == 9) { // #RRGGBBAA
            return Color::fromRGBA(
                (value >> 24) & 0xFF,
                (value >> 16) & 0xFF,
                (value >> 8) & 0xFF,
                value & 0xFF
            );
        }
        
        return Color();
    }
    
    // Common colors
    static constexpr Color White() { return Color(1, 1, 1, 1); }
    static constexpr Color Black() { return Color(0, 0, 0, 1); }
    static constexpr Color Red() { return Color(1, 0, 0, 1); }
    static constexpr Color Green() { return Color(0, 1, 0, 1); }
    static constexpr Color Blue() { return Color(0, 0, 1, 1); }
    static constexpr Color Yellow() { return Color(1, 1, 0, 1); }
    static constexpr Color Cyan() { return Color(0, 1, 1, 1); }
    static constexpr Color Magenta() { return Color(1, 0, 1, 1); }
};

} // namespace nexus::utility::graphics
