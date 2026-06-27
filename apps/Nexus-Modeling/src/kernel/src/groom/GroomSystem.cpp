#include "nexus/groom/GroomSystem.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace nexus::groom {
using Vec3 = nexus::render::Vec3;

GroomSystem::GroomSystem(const GroomConfig& c) : m_config(c) {}

void GroomSystem::generate() {
    m_strands.clear();
    std::mt19937 rng(m_config.strandCount);
    std::uniform_real_distribution<float> d(-1, 1);
    std::uniform_real_distribution<float> angle(0, 2 * std::numbers::pi_v<float>);

    std::vector<Vec3> guidePoints;
    for (uint32_t i = 0; i < m_config.clumps; ++i) {
        float a = angle(rng);
        float r = std::abs(d(rng)) * m_config.regionRadius;
        guidePoints.push_back(Vec3{m_config.regionOrigin.x + std::cos(a) * r,
                                    m_config.regionOrigin.y,
                                    m_config.regionOrigin.z + std::sin(a) * r});
    }

    for (uint32_t i = 0; i < m_config.strandCount; ++i) {
        GroomStrand s;
        s.thickness = m_config.thickness;
        s.length = m_config.length * (0.8f + std::abs(d(rng)) * 0.4f);

        Vec3 base;
        if (!guidePoints.empty()) {
            uint32_t ci = i % static_cast<uint32_t>(guidePoints.size());
            Vec3 guide = guidePoints[ci];
            float clumpOff = m_config.clumpStrength * d(rng);
            base = Vec3{guide.x + clumpOff, guide.y + clumpOff * 0.2f, guide.z + clumpOff};
        } else {
            float a = angle(rng);
            float r = std::abs(d(rng)) * m_config.regionRadius;
            base = Vec3{m_config.regionOrigin.x + std::cos(a) * r,
                        m_config.regionOrigin.y,
                        m_config.regionOrigin.z + std::sin(a) * r};
        }

        s.points.push_back(base);

        Vec3 dir{m_config.regionNormal.x + d(rng) * m_config.noiseAmount,
                 m_config.regionNormal.y + d(rng) * m_config.noiseAmount,
                 m_config.regionNormal.z + d(rng) * m_config.noiseAmount};
        float dLen = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (dLen > 1e-10f) {
            dir = Vec3{dir.x / dLen, dir.y / dLen, dir.z / dLen};
        }

        float segLen = s.length / static_cast<float>(m_config.segmentsPerStrand);
        for (uint32_t j = 1; j < m_config.segmentsPerStrand; ++j) {
            Vec3 pt{s.points.back().x + dir.x * segLen,
                    s.points.back().y + dir.y * segLen,
                    s.points.back().z + dir.z * segLen};
            dir = Vec3{dir.x + d(rng) * 0.1f, dir.y + d(rng) * 0.1f, dir.z + d(rng) * 0.1f};
            dLen = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (dLen > 1e-10f) {
                dir = Vec3{dir.x / dLen, dir.y / dLen, dir.z / dLen};
            }
            s.points.push_back(pt);
        }

        s.points.back() = Vec3{base.x + dir.x * s.length,
                                base.y + dir.y * s.length,
                                base.z + dir.z * s.length};
        m_strands.push_back(s);
    }
}

void GroomSystem::comb(const Vec3& pos, float radius, const Vec3& dir, float strength) {
    for (auto& s : m_strands) {
        for (auto& p : s.points) {
            float dx = p.x - pos.x;
            float dy = p.y - pos.y;
            float dz = p.z - pos.z;
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < radius) {
                float falloff = 1.0f - dist / radius;
                p = Vec3{p.x + dir.x * strength * falloff,
                         p.y + dir.y * strength * falloff,
                         p.z + dir.z * strength * falloff};
            }
        }
    }
}

void GroomSystem::brush(const Vec3& pos, float radius, const Vec3& dir, float strength) {
    comb(pos, radius, dir, strength);
}

void GroomSystem::cut(float length) {
    for (auto& s : m_strands) {
        float total = 0;
        size_t keep = 0;
        for (size_t i = 0; i + 1 < s.points.size(); ++i) {
            float dx = s.points[i + 1].x - s.points[i].x;
            float dy = s.points[i + 1].y - s.points[i].y;
            float dz = s.points[i + 1].z - s.points[i].z;
            total += std::sqrt(dx * dx + dy * dy + dz * dz);
            if (total < length) keep = i + 1;
        }
        if (keep + 1 < s.points.size()) {
            s.points.resize(keep + 1);
        }
    }
}

void GroomSystem::clump(float strength) {
    if (m_strands.size() < 2) return;

    std::vector<Vec3> centers;
    std::mt19937 rng(42);

    for (uint32_t i = 0; i < m_config.clumps; ++i) {
        if (i < m_strands.size()) {
            centers.push_back(m_strands[i].points[0]);
        }
    }

    for (size_t i = 0; i < m_strands.size(); ++i) {
        size_t ci = i % centers.size();
        Vec3& base = m_strands[i].points[0];
        float dx = centers[ci].x - base.x;
        float dy = centers[ci].y - base.y;
        float dz = centers[ci].z - base.z;
        base = Vec3{base.x + dx * strength, base.y + dy * strength, base.z + dz * strength};
    }
}

void GroomSystem::frizz(float amount) {
    std::mt19937 rng(42);
    std::normal_distribution<float> d(0, amount);

    for (auto& s : m_strands) {
        for (size_t i = 1; i < s.points.size(); ++i) {
            float t = static_cast<float>(i) / static_cast<float>(s.points.size());
            s.points[i] = Vec3{s.points[i].x + d(rng) * t,
                                s.points[i].y + d(rng) * t,
                                s.points[i].z + d(rng) * t};
        }
    }
}

void GroomSystem::smooth(uint32_t iterations) {
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (auto& s : m_strands) {
            for (size_t i = 1; i + 1 < s.points.size(); ++i) {
                s.points[i] = Vec3{(s.points[i - 1].x + s.points[i + 1].x) * 0.5f,
                                   (s.points[i - 1].y + s.points[i + 1].y) * 0.5f,
                                   (s.points[i - 1].z + s.points[i + 1].z) * 0.5f};
            }
        }
    }
}

geometry::Mesh GroomSystem::toMesh() const noexcept {
    geometry::Mesh m;
    std::vector<Vec3> pos;
    for (auto& s : m_strands) {
        for (auto& p : s.points) {
            pos.push_back(p);
        }
    }
    m.attributes().setPositions(pos);
    for (size_t i = 0; i < pos.size(); ++i) {
        geometry::Face f;
        f.indices = {static_cast<uint32_t>(i), static_cast<uint32_t>(i), static_cast<uint32_t>(i)};
        m.topology().addFace(f);
    }
    return m;
}

void GroomSystem::reset() {
    m_strands.clear();
}

}
