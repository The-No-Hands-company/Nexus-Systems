#include <nexus/parametric/ParametricSolver.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace nexus::parametric {

ParametricSolverReport ParametricSolver::solve(ConstraintGraph& graph,
                                               const ParametricSolverConfig& config) noexcept
{
    ParametricSolverReport report{};

    if (config.maxIterations == 0) {
        report.converged = false;
        report.errors.push_back("maxIterations must be greater than zero");
        return report;
    }

    if (config.convergenceEpsilon < 0.0) {
        report.converged = false;
        report.errors.push_back("convergenceEpsilon must be non-negative");
        return report;
    }

    if (graph.distanceConstraints().empty() &&
        graph.coincidentConstraints().empty() &&
        graph.axisAlignedDistanceConstraints().empty()) {
        return report;
    }

    report.converged = false;

    for (uint32_t iteration = 0; iteration < config.maxIterations; ++iteration) {
        double iterationMaxError = 0.0;

        for (const DistanceConstraint& constraint : graph.distanceConstraints()) {
            const ParametricPoint3* pointA = graph.point(constraint.entityA);
            const ParametricPoint3* pointB = graph.point(constraint.entityB);
            if (pointA == nullptr || pointB == nullptr) {
                report.errors.push_back("constraint references missing entity");
                continue;
            }

            const double dx = pointB->x - pointA->x;
            const double dy = pointB->y - pointA->y;
            const double dz = pointB->z - pointA->z;
            const double currentDistance = std::sqrt(dx * dx + dy * dy + dz * dz);

            const double error = std::fabs(currentDistance - constraint.targetDistance);
            iterationMaxError = std::max(iterationMaxError, error);

            ParametricPoint3 corrected{};
            if (currentDistance <= std::numeric_limits<double>::epsilon()) {
                corrected = *pointA;
                corrected.x += constraint.targetDistance;
            } else {
                const double invLen = 1.0 / currentDistance;
                corrected.x = pointA->x + dx * invLen * constraint.targetDistance;
                corrected.y = pointA->y + dy * invLen * constraint.targetDistance;
                corrected.z = pointA->z + dz * invLen * constraint.targetDistance;
            }

            if (!graph.setPoint(constraint.entityB, corrected)) {
                report.errors.push_back("failed to update constrained entity");
            }
        }

        for (const CoincidentConstraint& constraint : graph.coincidentConstraints()) {
            const ParametricPoint3* pointA = graph.point(constraint.entityA);
            const ParametricPoint3* pointB = graph.point(constraint.entityB);
            if (pointA == nullptr || pointB == nullptr) {
                report.errors.push_back("coincident constraint references missing entity");
                continue;
            }

            const double dx = pointB->x - pointA->x;
            const double dy = pointB->y - pointA->y;
            const double dz = pointB->z - pointA->z;
            iterationMaxError = std::max(iterationMaxError, std::sqrt(dx * dx + dy * dy + dz * dz));

            if (!graph.setPoint(constraint.entityB, *pointA)) {
                report.errors.push_back("failed to update coincident constrained entity");
            }
        }

        for (const AxisAlignedDistanceConstraint& constraint : graph.axisAlignedDistanceConstraints()) {
            const ParametricPoint3* pointA = graph.point(constraint.entityA);
            const ParametricPoint3* pointB = graph.point(constraint.entityB);
            if (pointA == nullptr || pointB == nullptr) {
                report.errors.push_back("axis-aligned constraint references missing entity");
                continue;
            }

            ParametricPoint3 corrected = *pointB;
            corrected.x = pointA->x;
            corrected.y = pointA->y;
            corrected.z = pointA->z;

            double axisError = 0.0;
            if (constraint.axis == Axis::X) {
                axisError = std::fabs((pointB->x - pointA->x) - constraint.targetDistance);
                corrected.x = pointA->x + constraint.targetDistance;
            } else if (constraint.axis == Axis::Y) {
                axisError = std::fabs((pointB->y - pointA->y) - constraint.targetDistance);
                corrected.y = pointA->y + constraint.targetDistance;
            } else {
                axisError = std::fabs((pointB->z - pointA->z) - constraint.targetDistance);
                corrected.z = pointA->z + constraint.targetDistance;
            }

            iterationMaxError = std::max(iterationMaxError, axisError);

            if (!graph.setPoint(constraint.entityB, corrected)) {
                report.errors.push_back("failed to update axis-aligned constrained entity");
            }
        }

        report.iterationsRan = iteration + 1;
        report.maxConstraintError = iterationMaxError;

        if (iterationMaxError <= config.convergenceEpsilon && report.errors.empty()) {
            report.converged = true;
            break;
        }
    }

    return report;
}

} // namespace nexus::parametric
