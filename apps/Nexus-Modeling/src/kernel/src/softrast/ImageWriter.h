#pragma once
#include "PixelBuffer.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::softrast {

inline bool writePPM(std::string_view path, const PixelBuffer& buf) {
    FILE* f = std::fopen(std::string(path).c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%u %u\n255\n", buf.width(), buf.height());
    for (const auto& px : buf.pixels()) {
        uint8_t rgb[3] = {px.r, px.g, px.b};
        if (std::fwrite(rgb, 1, 3, f) != 3) { std::fclose(f); return false; }
    }
    std::fclose(f);
    return true;
}

// ── Minimal, dependency-free PNG encoder ──────────────────────────────────────
//  Emits an 8-bit RGB PNG using zlib "stored" (uncompressed) blocks — no deflate
//  and no external libraries, so it links into the headless core. Files are larger
//  than a compressed PNG but decode in every standard viewer. Intended for the
//  developer visual-inspection loop (see tools/nxrender), not shipping assets.
namespace detail {

inline uint32_t pngCrc32(const uint8_t* data, std::size_t len) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        init = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

inline uint32_t pngAdler32(const uint8_t* data, std::size_t len) {
    constexpr uint32_t kMod = 65521u;
    uint32_t a = 1, b = 0;
    for (std::size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % kMod;
        b = (b + a) % kMod;
    }
    return (b << 16) | a;
}

inline void pngPutU32BE(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>((x >> 24) & 0xFFu));
    v.push_back(static_cast<uint8_t>((x >> 16) & 0xFFu));
    v.push_back(static_cast<uint8_t>((x >> 8) & 0xFFu));
    v.push_back(static_cast<uint8_t>(x & 0xFFu));
}

inline void pngWriteChunk(std::vector<uint8_t>& out, const char (&type)[5],
                          const std::vector<uint8_t>& data) {
    pngPutU32BE(out, static_cast<uint32_t>(data.size()));
    const std::size_t crcStart = out.size();
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(type[i]));
    out.insert(out.end(), data.begin(), data.end());
    pngPutU32BE(out, pngCrc32(out.data() + crcStart, out.size() - crcStart));
}

} // namespace detail

inline bool writePNG(std::string_view path, const PixelBuffer& buf) {
    const uint32_t W = buf.width(), H = buf.height();
    const auto& px = buf.pixels();

    // Filtered raw scanlines: each row prefixed with filter-type byte 0 (None).
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(H) * (1 + static_cast<std::size_t>(W) * 3));
    for (uint32_t y = 0; y < H; ++y) {
        raw.push_back(0);
        for (uint32_t x = 0; x < W; ++x) {
            const RGBA8& p = px[static_cast<std::size_t>(y) * W + x];
            raw.push_back(p.r);
            raw.push_back(p.g);
            raw.push_back(p.b);
        }
    }

    // zlib wrapper around stored (uncompressed) deflate blocks.
    std::vector<uint8_t> zlib;
    zlib.push_back(0x78);  // CMF: deflate, 32K window
    zlib.push_back(0x01);  // FLG: no dict, fastest
    std::size_t off = 0;
    while (off < raw.size() || off == 0) {
        const std::size_t chunk = std::min<std::size_t>(65535u, raw.size() - off);
        const bool last = (off + chunk >= raw.size());
        zlib.push_back(last ? 1u : 0u);
        const uint16_t len = static_cast<uint16_t>(chunk);
        const uint16_t nlen = static_cast<uint16_t>(~len);
        zlib.push_back(static_cast<uint8_t>(len & 0xFFu));
        zlib.push_back(static_cast<uint8_t>((len >> 8) & 0xFFu));
        zlib.push_back(static_cast<uint8_t>(nlen & 0xFFu));
        zlib.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFFu));
        zlib.insert(zlib.end(),
                    raw.begin() + static_cast<std::ptrdiff_t>(off),
                    raw.begin() + static_cast<std::ptrdiff_t>(off + chunk));
        off += chunk;
        if (last) break;
    }
    detail::pngPutU32BE(zlib, detail::pngAdler32(raw.data(), raw.size()));

    std::vector<uint8_t> out = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    detail::pngPutU32BE(ihdr, W);
    detail::pngPutU32BE(ihdr, H);
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(2);  // color type: RGB
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace
    detail::pngWriteChunk(out, "IHDR", ihdr);
    detail::pngWriteChunk(out, "IDAT", zlib);
    detail::pngWriteChunk(out, "IEND", {});

    FILE* f = std::fopen(std::string(path).c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

} // namespace nexus::softrast
