#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Parametric — constraint graph core
// ─────────────────────────────────────────────────────────────────────────────

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nexus::parametric {

using ParametricEntityId = uint64_t;
using ParametricConstraintId = uint64_t;

inline constexpr ParametricEntityId kInvalidEntityId = 0;
inline constexpr ParametricConstraintId kInvalidConstraintId = 0;

struct ParametricPoint3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct ParametricEntity {
    ParametricEntityId id = kInvalidEntityId;
    ParametricPoint3 point{};
};

enum class ConstraintType : uint8_t {
    Distance,
};

struct DistanceConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityA = kInvalidEntityId;
    ParametricEntityId entityB = kInvalidEntityId;
    double targetDistance = 0.0;
};

class ConstraintGraph {
public:
    [[nodiscard]] ParametricEntityId addPoint(const ParametricPoint3& point) noexcept;
    [[nodiscard]] bool removeEntity(ParametricEntityId id) noexcept;
    [[nodiscard]] bool hasEntity(ParametricEntityId id) const noexcept;

    [[nodiscard]] bool setPoint(ParametricEntityId id, const ParametricPoint3& point) noexcept;
    [[nodiscard]] const ParametricPoint3* point(ParametricEntityId id) const noexcept;

    [[nodiscard]] ParametricConstraintId addDistanceConstraint(ParametricEntityId entityA,
                                                               ParametricEntityId entityB,
                                                               double targetDistance) noexcept;
    [[nodiscard]] bool removeConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] bool hasConstraint(ParametricConstraintId id) const noexcept;

    [[nodiscard]] const std::vector<ParametricEntity>& entities() const noexcept { return m_entities; }
    [[nodiscard]] const std::vector<DistanceConstraint>& distanceConstraints() const noexcept
    {
        return m_distanceConstraints;
    }

    [[nodiscard]] size_t entityCount() const noexcept { return m_entities.size(); }
    [[nodiscard]] size_t distanceConstraintCount() const noexcept
    {
        return m_distanceConstraints.size();
    }

private:
    [[nodiscard]] std::vector<ParametricEntity>::iterator findEntity(ParametricEntityId id) noexcept;
    [[nodiscard]] std::vector<ParametricEntity>::const_iterator findEntity(ParametricEntityId id) const noexcept;
    [[nodiscard]] std::vector<DistanceConstraint>::iterator findConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<DistanceConstraint>::const_iterator findConstraint(ParametricConstraintId id) const noexcept;

    std::vector<ParametricEntity> m_entities;
    std::vector<DistanceConstraint> m_distanceConstraints;

    ParametricEntityId m_nextEntityId = 1;
    ParametricConstraintId m_nextConstraintId = 1;
};

} // namespace nexus::parametric
