// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Assembly Solver — 3D Assembly Constraint Resolution Impl
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/AssemblySolver.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <sstream>

namespace nexus::agent {

AssemblySolver::AssemblySolver() = default;

void AssemblySolver::addBody(const AssemblyBodyState& initialState)
{
    m_bodyStates.push_back(initialState);
}

void AssemblySolver::addConstraint(const AssemblyConstraint& constraint)
{
    m_constraints.push_back(constraint);
}

void AssemblySolver::setBodyFixed(uint32_t bodyIndex, bool fixed)
{
    if (bodyIndex < m_bodyStates.size()) m_bodyStates[bodyIndex].fixed = fixed;
}

Vec3 AssemblySolver::transformPoint(const Vec3& point, const AssemblyBodyState& state) const
{
    return {point.x + state.translation.x,
            point.y + state.translation.y,
            point.z + state.translation.z};
}

AssemblySolveResult AssemblySolver::solve(uint32_t maxIterations, float tolerance)
{
    AssemblySolveResult result;
    result.bodyStates = m_bodyStates;
    if (m_constraints.empty()) {
        result.converged = true;
        return result;
    }

    float adaptiveStep = 0.5f;
    auto& states = result.bodyStates;

    for (uint32_t iter = 0; iter < maxIterations; ++iter) {
        result.iterations = iter + 1;
        float maxConstraintError = 0.0f;

        for (const auto& c : m_constraints) {
            float error = 0.0f;
            Vec3 correction{};

            if (c.type == AssemblyConstraintType::Mate) {
                Vec3 pa = transformPoint(c.pointA, states[c.bodyA]);
                Vec3 pb = transformPoint(c.pointB, states[c.bodyB]);
                correction = pb - pa;
                error = correction.length();
            } else if (c.type == AssemblyConstraintType::Align) {
                Vec3 aA = c.axisA.normalize();
                Vec3 aB = c.axisB.normalize();
                correction = aA.cross(aB);
                error = correction.length();
            } else if (c.type == AssemblyConstraintType::Distance) {
                Vec3 pa = transformPoint(c.pointA, states[c.bodyA]);
                Vec3 pb = transformPoint(c.pointB, states[c.bodyB]);
                float current = (pb - pa).length();
                error = std::abs(current - c.value);
            } else if (c.type == AssemblyConstraintType::Angle) {
                Vec3 aA = c.axisA.normalize();
                Vec3 aB = c.axisB.normalize();
                float dot = std::clamp(aA.dot(aB), -1.0f, 1.0f);
                float current = std::acos(dot) * (180.0f / std::numbers::pi_v<float>);
                error = std::abs(current - c.value);
            } else if (c.type == AssemblyConstraintType::Gear) {
                Vec3 pa = transformPoint(c.pointA, states[c.bodyA]);
                Vec3 pb = transformPoint(c.pointB, states[c.bodyB]);
                float current = (pb - pa).length();
                error = std::abs(current - c.value);
            } else if (c.type == AssemblyConstraintType::Tangent) {
                Vec3 centerA = transformPoint(c.pointA, states[c.bodyA]);
                Vec3 centerB = transformPoint(c.pointB, states[c.bodyB]);
                float rA = c.axisA.length();
                float rB = c.axisB.length();
                float d = (centerB - centerA).length();
                error = std::abs(d - (rA + rB));
            } else {
                continue;
            }

            if (error > maxConstraintError) maxConstraintError = error;

            if (error > tolerance && !states[c.bodyB].fixed) {
                if (c.type == AssemblyConstraintType::Mate ||
                    c.type == AssemblyConstraintType::Align) {
                    states[c.bodyB].translation.x -= correction.x * adaptiveStep;
                    states[c.bodyB].translation.y -= correction.y * adaptiveStep;
                    states[c.bodyB].translation.z -= correction.z * adaptiveStep;
                } else if (c.type == AssemblyConstraintType::Distance) {
                    Vec3 pa = transformPoint(c.pointA, states[c.bodyA]);
                    Vec3 pb = transformPoint(c.pointB, states[c.bodyB]);
                    Vec3 dir2 = (pb - pa).normalize();
                    float currentDist = (pb - pa).length();
                    if (currentDist > 1e-10f) {
                        float delta = (c.value - currentDist) * adaptiveStep;
                        states[c.bodyB].translation.x += dir2.x * delta;
                        states[c.bodyB].translation.y += dir2.y * delta;
                        states[c.bodyB].translation.z += dir2.z * delta;
                    }
                }
            }
        }

        result.maxError = maxConstraintError;
        if (maxConstraintError <= tolerance) {
            result.converged = true;
            break;
        }

        adaptiveStep *= 0.95f;
    }

    for (const auto& c : m_constraints) {
        std::ostringstream oss;
        oss << "constraint_error type=" << static_cast<int>(c.type);
        result.messages.push_back(oss.str());
    }

    return result;
}

Vec3 AssemblySolver::evaluateMateError(const AssemblyConstraint& c) const
{
    Vec3 pa = transformPoint(c.pointA, m_bodyStates[c.bodyA]);
    Vec3 pb = transformPoint(c.pointB, m_bodyStates[c.bodyB]);
    return pb - pa;
}

Vec3 AssemblySolver::evaluateAlignError(const AssemblyConstraint& c) const
{
    Vec3 aA = c.axisA.normalize();
    Vec3 aB = c.axisB.normalize();
    return aA.cross(aB);
}

float AssemblySolver::evaluateDistanceError(const AssemblyConstraint& c) const
{
    Vec3 pa = transformPoint(c.pointA, m_bodyStates[c.bodyA]);
    Vec3 pb = transformPoint(c.pointB, m_bodyStates[c.bodyB]);
    float current = (pb - pa).length();
    return std::abs(current - c.value);
}

float AssemblySolver::evaluateAngleError(const AssemblyConstraint& c) const
{
    Vec3 aA = c.axisA.normalize();
    Vec3 aB = c.axisB.normalize();
    float dot = std::clamp(aA.dot(aB), -1.0f, 1.0f);
    float current = std::acos(dot) * (180.0f / std::numbers::pi_v<float>);
    return std::abs(current - c.value);
}

float AssemblySolver::evaluateGearError(const AssemblyConstraint& c) const
{
    Vec3 pa = transformPoint(c.pointA, m_bodyStates[c.bodyA]);
    Vec3 pb = transformPoint(c.pointB, m_bodyStates[c.bodyB]);
    float current = (pb - pa).length();
    return std::abs(current - c.value);
}

float AssemblySolver::evaluateTangentError(const AssemblyConstraint& c) const
{
    Vec3 centerA = transformPoint(c.pointA, m_bodyStates[c.bodyA]);
    Vec3 centerB = transformPoint(c.pointB, m_bodyStates[c.bodyB]);
    float rA = c.axisA.length();
    float rB = c.axisB.length();
    float d = (centerB - centerA).length();
    return std::abs(d - (rA + rB));
}

void AssemblySolver::clear()
{
    m_bodyStates.clear();
    m_constraints.clear();
}

} // namespace nexus::agent
