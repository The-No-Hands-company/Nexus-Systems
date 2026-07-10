#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Assembly Solver — 3D Assembly Constraint Resolution
//
//  Solves assembly constraints (mate, align, distance, angle, gear) by
//  iteratively adjusting body transforms until constraints are satisfied.
//  Uses the deterministic parametric solver for position-based constraints
//  and analytical methods for orientation-based constraints.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::agent {

using Vec3 = nexus::render::Vec3;

enum class AssemblyConstraintType : uint8_t {
    Mate,
    Align,
    Distance,
    Angle,
    Gear,
    Tangent,
    Count,
};

struct AssemblyConstraint {
    AssemblyConstraintType type = AssemblyConstraintType::Mate;
    uint32_t bodyA = 0;
    uint32_t bodyB = 0;
    // For mate/align: reference points/axes on each body
    Vec3 pointA{0, 0, 0};
    Vec3 pointB{0, 0, 0};
    Vec3 axisA{0, 1, 0};
    Vec3 axisB{0, 1, 0};
    float value = 0.0f; // distance for Distance, degrees for Angle, ratio for Gear
    nexus::parametric::ParametricConstraintId graphConstraintId = nexus::parametric::kInvalidConstraintId;
};

struct AssemblyBodyState {
    Vec3 translation{0, 0, 0};
    Vec3 rotation{0, 0, 0}; // Euler angles in radians
    bool fixed = false;
};

struct AssemblySolveResult {
    bool converged = false;
    uint32_t iterations = 0;
    float maxError = 0.0f;
    std::vector<AssemblyBodyState> bodyStates;
    std::vector<std::string> messages;
};

class AssemblySolver {
public:
    AssemblySolver();

    void addBody(const AssemblyBodyState& initialState);
    void addConstraint(const AssemblyConstraint& constraint);
    void setBodyFixed(uint32_t bodyIndex, bool fixed = true);

    [[nodiscard]] AssemblySolveResult solve(uint32_t maxIterations = 100, float tolerance = 1e-5f);

    [[nodiscard]] const std::vector<AssemblyBodyState>& bodyStates() const noexcept { return m_bodyStates; }
    [[nodiscard]] const std::vector<AssemblyConstraint>& constraints() const noexcept { return m_constraints; }
    [[nodiscard]] size_t bodyCount() const noexcept { return m_bodyStates.size(); }
    [[nodiscard]] size_t constraintCount() const noexcept { return m_constraints.size(); }

    void clear();

private:
    [[nodiscard]] Vec3 evaluateMateError(const AssemblyConstraint& c) const;
    [[nodiscard]] Vec3 evaluateAlignError(const AssemblyConstraint& c) const;
    [[nodiscard]] float evaluateDistanceError(const AssemblyConstraint& c) const;
    [[nodiscard]] float evaluateAngleError(const AssemblyConstraint& c) const;
    [[nodiscard]] float evaluateGearError(const AssemblyConstraint& c) const;
    [[nodiscard]] float evaluateTangentError(const AssemblyConstraint& c) const;

    [[nodiscard]] Vec3 transformPoint(const Vec3& point, const AssemblyBodyState& state) const;

    std::vector<AssemblyBodyState> m_bodyStates;
    std::vector<AssemblyConstraint> m_constraints;
};

} // namespace nexus::agent
