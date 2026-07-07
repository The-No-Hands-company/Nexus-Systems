#pragma once

#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {

/// @brief Draws debug overlays on pixel buffers for visual verification.
/// Draws bounding boxes, crosshairs, lines, and text labels directly
/// onto RGBA pixel data. Used to mark "where the math says things are"
/// so a vision AI can compare against the actual rendered output.

class VisualDebugOverlay {
public:
    using Vector3 = nexus::utility::math::Vector3;

    struct Color { uint8_t r, g, b, a; };
    struct Rect { int x, y, w, h; };
    struct ImageBuffer { uint8_t* data; int width; int height; int stride; };

    static constexpr Color Red{255,0,0,255}, Green{0,255,0,255}, Blue{0,0,255,255};
    static constexpr Color Yellow{255,255,0,255}, Cyan{0,255,255,255}, Magenta{255,0,255,255};
    static constexpr Color White{255,255,255,255}, Black{0,0,0,255};

    /// @brief Draw a 1-pixel rectangle outline.
    static void drawRect(ImageBuffer& img, const Rect& r, Color c) {
        drawHLine(img, r.x, r.y, r.w, c); drawHLine(img, r.x, r.y + r.h - 1, r.w, c);
        drawVLine(img, r.x, r.y, r.h, c); drawVLine(img, r.x + r.w - 1, r.y, r.h, c);
    }

    /// @brief Draw a filled rectangle.
    static void drawFilledRect(ImageBuffer& img, const Rect& r, Color c) {
        for (int row = 0; row < r.h; ++row)
            drawHLine(img, r.x, r.y + row, r.w, c);
    }

    /// @brief Draw a line using Bresenham's algorithm.
    static void drawLine(ImageBuffer& img, int x0, int y0, int x1, int y1, Color c) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            setPixel(img, x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    /// @brief Draw a 3x3 crosshair at a point.
    static void drawCrosshair(ImageBuffer& img, int x, int y, Color c, int size = 5) {
        drawHLine(img, x - size, y, size * 2 + 1, c);
        drawVLine(img, x, y - size, size * 2 + 1, c);
    }

    /// @brief Highlight a region with a semi-transparent overlay.
    static void highlightRegion(ImageBuffer& img, const Rect& r, Color c, uint8_t alpha = 80) {
        for (int row = r.y; row < r.y + r.h && row < img.height; ++row) {
            for (int col = r.x; col < r.x + r.w && col < img.width; ++col) {
                auto& p = pixel(img, col, row);
                p.r = static_cast<uint8_t>((p.r * (255 - alpha) + c.r * alpha) / 255);
                p.g = static_cast<uint8_t>((p.g * (255 - alpha) + c.g * alpha) / 255);
                p.b = static_cast<uint8_t>((p.b * (255 - alpha) + c.b * alpha) / 255);
            }
        }
    }

    /// @brief Draw a 3D bounding box wireframe projected to 2D.
    static void drawProjectedBox(ImageBuffer& img, const std::array<std::array<int,2>,8>& corners, Color c) {
        static const int edges[12][2] = {{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
        for (auto& e : edges)
            drawLine(img, corners[e[0]][0], corners[e[0]][1], corners[e[1]][0], corners[e[1]][1], c);
    }

    /// @brief Draw text label (simple 5x7 font placeholder - draws underline instead).
    static void drawLabel(ImageBuffer& img, int x, int y, const std::string& text, Color c) {
        drawHLine(img, x, y + 7, static_cast<int>(text.size()) * 6, c);
    }

    /// @brief Compare two images and overlay differences in red.
    /// Returns number of differing pixels.
    static int drawDiff(ImageBuffer& img, const ImageBuffer& reference, uint8_t tolerance = 10) {
        int diffs = 0;
        for (int row = 0; row < std::min(img.height, reference.height); ++row) {
            for (int col = 0; col < std::min(img.width, reference.width); ++col) {
                auto a = pixel(reference, col, row), b = pixel(img, col, row);
                if (std::abs(a.r - b.r) > tolerance || std::abs(a.g - b.g) > tolerance ||
                    std::abs(a.b - b.b) > tolerance || std::abs(a.a - b.a) > tolerance) {
                    setPixel(img, col, row, Red); ++diffs;
                }
            }
        }
        return diffs;
    }

    /// @brief Draw a grid pattern for spatial reference.
    static void drawGrid(ImageBuffer& img, int spacing, Color c, uint8_t alpha = 40) {
        Color gc{c.r, c.g, c.b, alpha};
        for (int x = 0; x < img.width; x += spacing) drawVLine(img, x, 0, img.height, gc);
        for (int y = 0; y < img.height; y += spacing) drawHLine(img, 0, y, img.width, gc);
    }

private:
    static void setPixel(ImageBuffer& img, int x, int y, Color c) {
        if (x < 0 || x >= img.width || y < 0 || y >= img.height) return;
        auto& p = pixel(img, x, y);
        p.r = c.r; p.g = c.g; p.b = c.b; p.a = c.a;
    }
    static Color& pixel(ImageBuffer& img, int x, int y) {
        return *reinterpret_cast<Color*>(img.data + y * img.stride + x * 4);
    }
    static const Color& pixel(const ImageBuffer& img, int x, int y) {
        return *reinterpret_cast<const Color*>(img.data + y * img.stride + x * 4);
    }
    static void drawHLine(ImageBuffer& img, int x, int y, int len, Color c) {
        for (int i = 0; i < len; ++i) setPixel(img, x + i, y, c);
    }
    static void drawVLine(ImageBuffer& img, int x, int y, int len, Color c) {
        for (int i = 0; i < len; ++i) setPixel(img, x, y + i, c);
    }
};

/// @brief Compares two rendered images and produces a visual regression report.
class RenderComparator {
public:
    struct DiffReport {
        int totalPixels = 0;
        int diffPixels = 0;
        double diffPercent = 0.0;
        int maxDiff = 0;
        double avgDiff = 0.0;
        std::vector<std::array<int,2>> diffRegions; // {x,y} of significant diff clusters
    };

    /// @brief Compare two image buffers pixel by pixel.
    [[nodiscard]] static DiffReport compare(const VisualDebugOverlay::ImageBuffer& a,
                                             const VisualDebugOverlay::ImageBuffer& b,
                                             uint8_t tolerance = 10) {
        DiffReport r;
        r.totalPixels = std::min(a.width, b.width) * std::min(a.height, b.height);
        int sumDiff = 0;
        for (int y = 0; y < std::min(a.height, b.height); ++y) {
            for (int x = 0; x < std::min(a.width, b.width); ++x) {
                auto pa = pixelAt(a, x, y), pb = pixelAt(b, x, y);
                int dr = std::abs(pa.r - pb.r), dg = std::abs(pa.g - pb.g);
                int db = std::abs(pa.b - pb.b), da = std::abs(pa.a - pb.a);
                int diff = std::max({dr, dg, db, da});
                if (diff > tolerance) {
                    ++r.diffPixels;
                    r.maxDiff = std::max(r.maxDiff, diff);
                    sumDiff += diff;
                    if (r.diffRegions.size() < 100 && (r.diffPixels % 100 == 0))
                        r.diffRegions.push_back({x, y});
                }
            }
        }
        if (r.totalPixels > 0) {
            r.diffPercent = 100.0 * r.diffPixels / r.totalPixels;
            r.avgDiff = r.diffPixels > 0 ? static_cast<double>(sumDiff) / r.diffPixels : 0.0;
        }
        return r;
    }

    /// @brief Generate a human-readable report string.
    [[nodiscard]] static std::string reportString(const DiffReport& r) {
        std::ostringstream s;
        s << "=== Visual Diff Report ===\n"
          << "  Total pixels:  " << r.totalPixels << "\n"
          << "  Diff pixels:   " << r.diffPixels << " (" << r.diffPercent << "%)\n"
          << "  Max diff:      " << r.maxDiff << "\n"
          << "  Avg diff:      " << r.avgDiff << "\n";
        if (r.diffPercent > 1.0)
            s << "  Status:        FAILED (>" << (r.diffPercent > 5.0 ? "5" : "1") << "% threshold)\n";
        else if (r.diffPercent > 0.1)
            s << "  Status:        WARNING\n";
        else
            s << "  Status:        PASSED\n";
        s << "========================\n";
        return s.str();
    }

private:
    static VisualDebugOverlay::Color pixelAt(const VisualDebugOverlay::ImageBuffer& img, int x, int y) {
        return *reinterpret_cast<const VisualDebugOverlay::Color*>(img.data + y * img.stride + x * 4);
    }
};

} // namespace nexus::utility::graphics
