#include <nexus/render/ReflectionCapture.h>

#include <algorithm>
#include <cmath>

namespace nexus::render {
using Vec3 = nexus::render::Vec3;

static Vec3 cubemapDirection(uint32_t face, uint32_t x, uint32_t y, uint32_t res) {
    float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(res) * 2.0f - 1.0f;
    float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(res) * 2.0f - 1.0f;

    Vec3 dir{};
    switch (face) {
        case 0: dir = Vec3{ 1.0f,  v, -u}; break;
        case 1: dir = Vec3{-1.0f,  v,  u}; break;
        case 2: dir = Vec3{ u,  1.0f, -v}; break;
        case 3: dir = Vec3{ u, -1.0f,  v}; break;
        case 4: dir = Vec3{ u,  v,  1.0f}; break;
        case 5: dir = Vec3{-u,  v, -1.0f}; break;
    }

    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 1e-10f) {
        dir = Vec3{dir.x / len, dir.y / len, dir.z / len};
    }
    return dir;
}

std::vector<float> ReflectionCapture::capture(const Vec3& pos,
                                               const std::function<Vec3(const Vec3&)>& sampleFn,
                                               uint32_t res) {
    (void)pos;
    size_t totalPixels = static_cast<size_t>(res * res * 6);
    std::vector<float> cubemap(totalPixels * 3, 0.0f);

    for (uint32_t face = 0; face < 6; ++face) {
        for (uint32_t y = 0; y < res; ++y) {
            for (uint32_t x = 0; x < res; ++x) {
                Vec3 dir = cubemapDirection(face, x, y, res);
                Vec3 color = sampleFn(dir);

                size_t pixelIdx = (static_cast<size_t>(face) * res * res +
                                   static_cast<size_t>(y) * res + x) * 3;
                cubemap[pixelIdx]     = std::clamp(color.x, 0.0f, 1.0f);
                cubemap[pixelIdx + 1] = std::clamp(color.y, 0.0f, 1.0f);
                cubemap[pixelIdx + 2] = std::clamp(color.z, 0.0f, 1.0f);
            }
        }
    }

    return cubemap;
}

std::vector<float> ReflectionCapture::sample(const std::vector<float>& cubemap,
                                              uint32_t res,
                                              const Vec3& direction) {
    float absX = std::abs(direction.x);
    float absY = std::abs(direction.y);
    float absZ = std::abs(direction.z);

    uint32_t face;
    float sc, tc;
    float ma;

    if (absX >= absY && absX >= absZ) {
        ma = absX;
        if (direction.x > 0) {
            face = 0;
            sc = -direction.z;
            tc = -direction.y;
        } else {
            face = 1;
            sc =  direction.z;
            tc = -direction.y;
        }
    } else if (absY >= absX && absY >= absZ) {
        ma = absY;
        if (direction.y > 0) {
            face = 2;
            sc =  direction.x;
            tc =  direction.z;
        } else {
            face = 3;
            sc =  direction.x;
            tc = -direction.z;
        }
    } else {
        ma = absZ;
        if (direction.z > 0) {
            face = 4;
            sc =  direction.x;
            tc = -direction.y;
        } else {
            face = 5;
            sc = -direction.x;
            tc = -direction.y;
        }
    }

    float u = (sc / ma + 1.0f) * 0.5f;
    float v = (tc / ma + 1.0f) * 0.5f;

    uint32_t px = std::clamp(static_cast<uint32_t>(u * static_cast<float>(res - 1)), 0u, res - 1);
    uint32_t py = std::clamp(static_cast<uint32_t>(v * static_cast<float>(res - 1)), 0u, res - 1);

    size_t idx = (static_cast<size_t>(face) * res * res + static_cast<size_t>(py) * res + px) * 3;
    if (idx + 2 >= cubemap.size()) return {0.5f, 0.5f, 0.5f};

    return {cubemap[idx], cubemap[idx + 1], cubemap[idx + 2]};
}

} // namespace nexus::render
