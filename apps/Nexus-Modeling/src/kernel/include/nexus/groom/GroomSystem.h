#pragma once
#include <nexus/render/Camera.h>
#include <nexus/geometry/Mesh.h>
#include <cstdint>
#include <vector>
namespace nexus::groom {
using Vec3 = nexus::render::Vec3;
struct GroomStrand { std::vector<Vec3> points; float thickness=0.01f; float length=1; };
struct GroomConfig { uint32_t strandCount=500,segmentsPerStrand=8; float length=1,thickness=0.02f; float noiseAmount=0.1f,clumpStrength=0.5f; uint32_t clumps=10; Vec3 regionNormal{0,1,0}; Vec3 regionOrigin{}; float regionRadius=1; };
class GroomSystem {
public:
    explicit GroomSystem(const GroomConfig& c = GroomConfig{});
    void generate();
    void setConfig(const GroomConfig& c) { m_config = c; }
    [[nodiscard]] geometry::Mesh toMesh() const noexcept;
    [[nodiscard]] const std::vector<GroomStrand>& strands() const noexcept { return m_strands; }
    void comb(const Vec3& pos, float radius, const Vec3& dir, float strength);
    void brush(const Vec3& pos, float radius, const Vec3& dir, float strength);
    void cut(float length);
    void clump(float strength);
    void frizz(float amount);
    void smooth(uint32_t iterations = 3);
    void reset();
private:
    GroomConfig m_config;
    std::vector<GroomStrand> m_strands;
    std::vector<Vec3> m_guidePoints;
    void generateGuides();
};
}
