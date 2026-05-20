#include <nexus/parametric/ParametricSamples.h>

#include <bit>
#include <cstdint>

namespace nexus::parametric {

namespace {

bool isFiniteDouble(double value) noexcept
{
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t kExpMask = 0x7FF0000000000000ULL;
    return (bits & kExpMask) != kExpMask;
}

} // namespace

SketchSampleModel ParametricSampleGenerator::makeSketchRectangle(double width, double height)
{
    SketchSampleModel model{};

    if (!isFiniteDouble(width) || !isFiniteDouble(height)) {
        return model;
    }

    // Seed with intentionally non-conforming positions so the solver does real work.
    model.origin = model.graph.addPoint({0.2, 0.3, 1.0});
    model.xHandle = model.graph.addPoint({5.0, 7.0, -2.0});
    model.yHandle = model.graph.addPoint({-3.0, 4.0, 9.0});
    model.corner = model.graph.addPoint({8.0, 6.0, 5.0});

    // Anchor origin at world origin by direct assignment.
    if (!model.graph.setPoint(model.origin, {0.0, 0.0, 0.0})) {
        return model;
    }

    // Width and height axis handles from origin.
    if (model.graph.addAxisAlignedDistanceConstraint(model.origin, model.xHandle, Axis::X, width) ==
        kInvalidConstraintId) {
        return model;
    }
    if (model.graph.addAxisAlignedDistanceConstraint(model.origin, model.yHandle, Axis::Y, height) ==
        kInvalidConstraintId) {
        return model;
    }

    // Corner shares x with x-handle and y with y-handle.
    if (model.graph.addAxisAlignedDistanceConstraint(model.xHandle, model.corner, Axis::Y, height) ==
        kInvalidConstraintId) {
        return model;
    }
    if (model.graph.addAxisAlignedDistanceConstraint(model.yHandle, model.corner, Axis::X, width) ==
        kInvalidConstraintId) {
        return model;
    }
    return model;
}

ParametricSolverReport ParametricSampleGenerator::solveSketchRectangle(SketchSampleModel& model,
                                                                       const ParametricSolverConfig& config) noexcept
{
    if (config.maxIterations == 0u) {
        ParametricSolverReport report{};
        report.converged = false;
        report.errors.push_back("maxIterations must be greater than zero");
        return report;
    }

    return ParametricSolver::solve(model.graph, config);
}

} // namespace nexus::parametric
