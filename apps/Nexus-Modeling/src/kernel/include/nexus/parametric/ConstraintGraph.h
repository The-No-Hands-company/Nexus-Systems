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
    Coincident,
    AxisAlignedDistance,
};

enum class Axis : uint8_t {
    X,
    Y,
    Z,
};

struct DistanceConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityA = kInvalidEntityId;
    ParametricEntityId entityB = kInvalidEntityId;
    double targetDistance = 0.0;
};

struct CoincidentConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityA = kInvalidEntityId;
    ParametricEntityId entityB = kInvalidEntityId;
};

struct AxisAlignedDistanceConstraint {
    ParametricConstraintId id = kInvalidConstraintId;
    ParametricEntityId entityA = kInvalidEntityId;
    ParametricEntityId entityB = kInvalidEntityId;
    Axis axis = Axis::X;
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
    [[nodiscard]] ParametricConstraintId addCoincidentConstraint(ParametricEntityId entityA,
                                                                 ParametricEntityId entityB) noexcept;
    [[nodiscard]] ParametricConstraintId addAxisAlignedDistanceConstraint(ParametricEntityId entityA,
                                                                          ParametricEntityId entityB,
                                                                          Axis axis,
                                                                          double targetDistance) noexcept;
    [[nodiscard]] bool removeConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] bool hasConstraint(ParametricConstraintId id) const noexcept;

    [[nodiscard]] const std::vector<ParametricEntity>& entities() const noexcept { return m_entities; }
    [[nodiscard]] const std::vector<DistanceConstraint>& distanceConstraints() const noexcept
    {
        return m_distanceConstraints;
    }
    [[nodiscard]] const std::vector<CoincidentConstraint>& coincidentConstraints() const noexcept
    {
        return m_coincidentConstraints;
    }
    [[nodiscard]] const std::vector<AxisAlignedDistanceConstraint>& axisAlignedDistanceConstraints() const noexcept
    {
        return m_axisAlignedDistanceConstraints;
    }

    [[nodiscard]] size_t entityCount() const noexcept { return m_entities.size(); }
    [[nodiscard]] size_t distanceConstraintCount() const noexcept
    {
        return m_distanceConstraints.size();
    }
    [[nodiscard]] size_t coincidentConstraintCount() const noexcept
    {
        return m_coincidentConstraints.size();
    }
    [[nodiscard]] size_t axisAlignedDistanceConstraintCount() const noexcept
    {
        return m_axisAlignedDistanceConstraints.size();
    }

private:
    [[nodiscard]] std::vector<ParametricEntity>::iterator findEntity(ParametricEntityId id) noexcept;
    [[nodiscard]] std::vector<ParametricEntity>::const_iterator findEntity(ParametricEntityId id) const noexcept;
    [[nodiscard]] std::vector<DistanceConstraint>::iterator findConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<DistanceConstraint>::const_iterator findConstraint(ParametricConstraintId id) const noexcept;
    [[nodiscard]] std::vector<CoincidentConstraint>::iterator findCoincidentConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<CoincidentConstraint>::const_iterator findCoincidentConstraint(ParametricConstraintId id) const noexcept;
    [[nodiscard]] std::vector<AxisAlignedDistanceConstraint>::iterator findAxisAlignedDistanceConstraint(ParametricConstraintId id) noexcept;
    [[nodiscard]] std::vector<AxisAlignedDistanceConstraint>::const_iterator findAxisAlignedDistanceConstraint(ParametricConstraintId id) const noexcept;

    std::vector<ParametricEntity> m_entities;
    std::vector<DistanceConstraint> m_distanceConstraints;
    std::vector<CoincidentConstraint> m_coincidentConstraints;
    std::vector<AxisAlignedDistanceConstraint> m_axisAlignedDistanceConstraints;

    ParametricEntityId m_nextEntityId = 1;
    ParametricConstraintId m_nextConstraintId = 1;
};

} // namespace nexus::parametric
