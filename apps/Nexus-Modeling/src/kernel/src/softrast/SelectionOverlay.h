#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Softrast — Selection Overlay
//
//  Industry-standard selection visualisation for the headless preview path:
//  a screen-space silhouette OUTLINE driven by an object-ID buffer, with distinct
//  hover / selected / active styling (cf. Blender selected-orange + active-white,
//  Maya preselection highlight).
//
//  Technique: the Jump-Flood Algorithm (JFA) builds a distance field from the
//  silhouettes of all "stateful" objects in O(N log N); a band of width W just
//  OUTSIDE each silhouette is tinted with that object's state colour. JFA gives
//  crisp, arbitrarily-wide outlines cheaply — the same approach shipped in
//  real-time engines.
// ─────────────────────────────────────────────────────────────────────────────
#include "PixelBuffer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::softrast {

enum class SelState : uint8_t { None = 0, Hover = 1, Selected = 2, Active = 3 };

struct OutlineStyle {
    RGBA8 hover{205, 209, 217, 255};    // pale pre-highlight
    RGBA8 selected{255, 124, 48, 255};  // selected — Blender-family orange
    RGBA8 active{250, 251, 255, 255};   // active — near white
    float hoverWidth = 2.0f;
    float selectedWidth = 3.5f;
    float activeWidth = 3.5f;
    float aa = 1.25f;  // anti-aliased falloff (px) past the solid band
};

// Composite a selection outline over `color`, reading the front-most object per
// pixel from `id` (0 = background) and each object's state from `stateByObject`
// (indexed by objectId-1). Outline colour/width follow the object's state.
inline void applySelectionOutline(PixelBuffer& color, const std::vector<uint32_t>& id,
                                  uint32_t W, uint32_t H,
                                  const std::vector<SelState>& stateByObject,
                                  const OutlineStyle& style = {})
{
    const size_t N = static_cast<size_t>(W) * H;
    if (N == 0 || id.size() != N) return;

    auto stateOf = [&](uint32_t objId) -> SelState {
        if (objId == 0) return SelState::None;
        const size_t i = objId - 1u;
        return (i < stateByObject.size()) ? stateByObject[i] : SelState::None;
    };

    // ── Seed the field: a pixel is a seed iff its object has a non-None state ──
    std::vector<int32_t> sx(N, -1), sy(N, -1);
    bool anySeed = false;
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            const size_t p = static_cast<size_t>(y) * W + x;
            if (stateOf(id[p]) != SelState::None) {
                sx[p] = static_cast<int32_t>(x);
                sy[p] = static_cast<int32_t>(y);
                anySeed = true;
            }
        }
    }
    if (!anySeed) return;

    // ── Jump flood: propagate the nearest seed, halving the step each pass ────
    std::vector<int32_t> tx(sx), ty(sy);
    const int iW = static_cast<int>(W), iH = static_cast<int>(H);
    uint32_t step = 1;
    while (step * 2u < ((W > H) ? W : H)) step *= 2u;
    for (; step >= 1u; step >>= 1) {
        const int s = static_cast<int>(step);
        for (int y = 0; y < iH; ++y) {
            for (int x = 0; x < iW; ++x) {
                const size_t p = static_cast<size_t>(y) * iW + x;
                int bX = sx[p], bY = sy[p];
                double bD = (bX < 0) ? 1e30
                    : static_cast<double>((x - bX) * (x - bX) + (y - bY) * (y - bY));
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        const int nx = x + ox * s, ny = y + oy * s;
                        if (nx < 0 || ny < 0 || nx >= iW || ny >= iH) continue;
                        const size_t q = static_cast<size_t>(ny) * iW + nx;
                        if (sx[q] < 0) continue;
                        const int ddx = x - sx[q], ddy = y - sy[q];
                        const double d = static_cast<double>(ddx * ddx + ddy * ddy);
                        if (d < bD) { bD = d; bX = sx[q]; bY = sy[q]; }
                    }
                }
                tx[p] = bX; ty[p] = bY;
            }
        }
        sx.swap(tx); sy.swap(ty);
    }

    // ── Composite the outline band just outside each silhouette ───────────────
    for (int y = 0; y < iH; ++y) {
        for (int x = 0; x < iW; ++x) {
            const size_t p = static_cast<size_t>(y) * iW + x;
            // Keep the outline OUTSIDE outlined objects: only paint background or
            // pixels belonging to a non-outlined object (lets it draw over others,
            // as DCC selection outlines do).
            if (stateOf(id[p]) != SelState::None) continue;
            const int seedX = sx[p]; if (seedX < 0) continue;
            const int seedY = sy[p];
            const float d = std::sqrt(static_cast<float>(
                (x - seedX) * (x - seedX) + (y - seedY) * (y - seedY)));
            if (d <= 0.0f) continue;

            const SelState st = stateOf(id[static_cast<size_t>(seedY) * iW + seedX]);
            RGBA8 oc; float width;
            switch (st) {
                case SelState::Hover:    oc = style.hover;    width = style.hoverWidth;    break;
                case SelState::Selected: oc = style.selected; width = style.selectedWidth; break;
                case SelState::Active:   oc = style.active;   width = style.activeWidth;   break;
                default: continue;
            }
            if (d > width + style.aa) continue;
            float alpha = 1.0f - std::max(0.0f, (d - width) / std::max(0.001f, style.aa));
            alpha = std::clamp(alpha, 0.0f, 1.0f);
            if (alpha <= 0.0f) continue;

            const RGBA8 b = color.getPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
            color.setPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y), {
                static_cast<uint8_t>(b.r + (oc.r - b.r) * alpha + 0.5f),
                static_cast<uint8_t>(b.g + (oc.g - b.g) * alpha + 0.5f),
                static_cast<uint8_t>(b.b + (oc.b - b.b) * alpha + 0.5f), 255});
        }
    }
}

} // namespace nexus::softrast
