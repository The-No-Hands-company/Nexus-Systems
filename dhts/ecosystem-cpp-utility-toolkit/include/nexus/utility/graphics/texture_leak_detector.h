#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <algorithm>

namespace nexus::utility::graphics {

/**
 * @brief Tracks GPU texture allocations to detect leaks (never-freed textures).
 *
 * Backend-agnostic (OpenGL / Vulkan): identify textures by their handle/id and
 * estimate memory footprint from format, dimensions, and mip count.
 */
class TextureLeakDetector {
public:
    struct TextureInfo {
        std::uint32_t id = 0;
        std::string name;
        std::size_t width = 0;
        std::size_t height = 0;
        std::string format;
        std::size_t estimated_bytes = 0;
    };

    TextureLeakDetector() = default;

    /**
     * @brief Records a texture allocation.
     */
    void registerTexture(std::uint32_t id, std::size_t width, std::size_t height,
                         std::size_t mips, const std::string& format,
                         const std::string& name) {
        TextureInfo info;
        info.id = id;
        info.name = name;
        info.width = width;
        info.height = height;
        info.format = format;
        info.estimated_bytes = estimateBytes(width, height, mips, format);
        textures_[id] = std::move(info);
    }

    /**
     * @brief Records a texture free; safe to call for unknown ids.
     */
    void unregisterTexture(std::uint32_t id) {
        textures_.erase(id);
    }

    std::size_t getActiveTextureCount() const {
        return textures_.size();
    }

    /**
     * @brief Total estimated bytes of all currently-live textures.
     */
    std::size_t getTotalTextureMemory() const {
        std::size_t total = 0;
        for (const auto& kv : textures_) total += kv.second.estimated_bytes;
        return total;
    }

    /**
     * @brief Returns all textures still registered (i.e. leaked), sorted by id.
     */
    std::vector<TextureInfo> detectLeaks() const {
        std::vector<TextureInfo> leaks;
        leaks.reserve(textures_.size());
        for (const auto& kv : textures_) leaks.push_back(kv.second);
        std::sort(leaks.begin(), leaks.end(),
                  [](const TextureInfo& a, const TextureInfo& b) { return a.id < b.id; });
        return leaks;
    }

    void clear() { textures_.clear(); }

    /**
     * @brief Bytes-per-pixel estimate for a named GPU format (defaults to 4).
     */
    static std::size_t bytesPerPixel(const std::string& format) {
        std::string f = toUpper(format);
        // 16 bytes / pixel
        if (contains(f, "RGBA32F") || contains(f, "RGBA32") ) return 16;
        // 8 bytes / pixel
        if (contains(f, "RGBA16") || contains(f, "RG32")) return 8;
        // 4 bytes / pixel
        if (contains(f, "RGBA8") || contains(f, "RGBA") ||
            contains(f, "BGRA") || contains(f, "RG16") ||
            contains(f, "R32") || contains(f, "DEPTH24") ||
            contains(f, "DEPTH32") || contains(f, "D24") || contains(f, "D32")) return 4;
        // 3 bytes / pixel
        if (contains(f, "RGB8") || contains(f, "RGB")) return 3;
        // 2 bytes / pixel
        if (contains(f, "RG8") || contains(f, "R16") ||
            contains(f, "DEPTH16") || contains(f, "D16")) return 2;
        // 1 byte / pixel
        if (contains(f, "R8") || contains(f, "A8")) return 1;
        return 4; // sensible default
    }

private:
    static std::size_t estimateBytes(std::size_t width, std::size_t height,
                                     std::size_t mips, const std::string& format) {
        if (width == 0 || height == 0) return 0;
        const std::size_t bpp = bytesPerPixel(format);
        if (mips == 0) mips = 1;

        std::size_t total = 0;
        std::size_t w = width;
        std::size_t h = height;
        for (std::size_t m = 0; m < mips; ++m) {
            total += w * h * bpp;
            if (w == 1 && h == 1) break;
            w = (w > 1) ? w / 2 : 1;
            h = (h > 1) ? h / 2 : 1;
        }
        return total;
    }

    static std::string toUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return s;
    }

    static bool contains(const std::string& haystack, const char* needle) {
        return haystack.find(needle) != std::string::npos;
    }

    std::unordered_map<std::uint32_t, TextureInfo> textures_;
};

} // namespace nexus::utility::graphics
