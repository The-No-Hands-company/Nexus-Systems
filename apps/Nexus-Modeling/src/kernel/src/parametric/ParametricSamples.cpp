#include <nexus/parametric/ParametricSamples.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace nexus::parametric {

namespace {
bool isFiniteDouble(double value) noexcept {
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t kExpMask = 0x7FF0000000000000ULL;
    return (bits & kExpMask) != kExpMask;
}
}

SketchSampleModel ParametricSampleGenerator::makeSketchRectangle(double width, double height) {
    SketchSampleModel model{};

    if (!isFiniteDouble(width) || !isFiniteDouble(height)) return model;

    model.origin = model.graph.addPoint({0.2, 0.3, 1.0});
    model.xHandle = model.graph.addPoint({5.0, 7.0, -2.0});
    model.yHandle = model.graph.addPoint({-3.0, 4.0, 9.0});
    model.corner = model.graph.addPoint({8.0, 6.0, 5.0});

    if (!model.graph.setPoint(model.origin, {0.0, 0.0, 0.0})) return model;

    if (model.graph.addAxisAlignedDistanceConstraint(model.origin, model.xHandle, Axis::X, width) == kInvalidConstraintId) return model;
    if (model.graph.addAxisAlignedDistanceConstraint(model.origin, model.yHandle, Axis::Y, height) == kInvalidConstraintId) return model;
    if (model.graph.addAxisAlignedDistanceConstraint(model.xHandle, model.corner, Axis::Y, height) == kInvalidConstraintId) return model;
    if (model.graph.addAxisAlignedDistanceConstraint(model.yHandle, model.corner, Axis::X, width) == kInvalidConstraintId) return model;

    return model;
}

SketchSampleModel ParametricSampleGenerator::makeSketchCircle(double radius, uint32_t segments) {
    SketchSampleModel model{};
    if (!isFiniteDouble(radius) || radius <= 0.0 || segments < 3) return model;

    model.origin = model.graph.addPoint({0.0, 0.0, 0.0});

    double angleStep = 2.0 * std::numbers::pi / static_cast<double>(segments);
    std::vector<ParametricEntityId> circlePoints;

    for (uint32_t i = 0; i < segments; ++i) {
        double angle = static_cast<double>(i) * angleStep;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        auto pt = model.graph.addPoint({x, y, 0.0});
        circlePoints.push_back(pt);

        (void)model.graph.addDistanceConstraint(model.origin, pt, radius);

        if (i > 0) {
            double dx = x - radius * std::cos(angle - angleStep);
            double dy = y - radius * std::sin(angle - angleStep);
            double chordLen = std::sqrt(dx * dx + dy * dy);
            (void)model.graph.addDistanceConstraint(circlePoints[i - 1], pt, chordLen);
        }
    }

    double dx2 = 0.0;
    (void)model.graph.addDistanceConstraint(circlePoints.back(), circlePoints[0],
        std::sqrt(dx2));

    model.xHandle = model.origin;
    model.yHandle = circlePoints.empty() ? model.origin : circlePoints[0];
    model.corner = circlePoints.empty() ? model.origin : circlePoints[static_cast<size_t>(segments / 4)];

    return model;
}

SketchSampleModel ParametricSampleGenerator::makeSketchArc(double radius, double startAngle, double endAngle,
                                                             uint32_t segments) {
    SketchSampleModel model{};
    if (!isFiniteDouble(radius) || radius <= 0.0 || segments < 2) return model;

    model.origin = model.graph.addPoint({0.0, 0.0, 0.0});

    double startRad = startAngle * std::numbers::pi / 180.0;
    double endRad = endAngle * std::numbers::pi / 180.0;
    double span = endRad - startRad;
    double angleStep = span / static_cast<double>(segments - 1);

    std::vector<ParametricEntityId> arcPoints;
    for (uint32_t i = 0; i < segments; ++i) {
        double angle = startRad + static_cast<double>(i) * angleStep;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        auto pt = model.graph.addPoint({x, y, 0.0});
        arcPoints.push_back(pt);

        (void)model.graph.addDistanceConstraint(model.origin, pt, radius);

        if (i > 0) {
            double prevAngle = startRad + static_cast<double>(i - 1) * angleStep;
            double dx = radius * std::cos(angle) - radius * std::cos(prevAngle);
            double dy = radius * std::sin(angle) - radius * std::sin(prevAngle);
            double chordLen = std::sqrt(dx * dx + dy * dy);
            (void)model.graph.addDistanceConstraint(arcPoints[i - 1], pt, chordLen);
        }
    }

    if (arcPoints.size() >= 2) {
        model.xHandle = arcPoints.front();
        model.yHandle = arcPoints.back();
        model.corner = arcPoints[arcPoints.size() / 2];
    } else {
        model.xHandle = model.origin;
        model.yHandle = model.origin;
        model.corner = model.origin;
    }

    return model;
}

SketchSampleModel ParametricSampleGenerator::makeSketchSlot(double length, double width, uint32_t segments) {
    SketchSampleModel model{};
    if (!isFiniteDouble(length) || !isFiniteDouble(width) || segments < 4) return model;

    double halfW = width * 0.5;
    double straightLen = length - width;
    double radius = halfW;

    model.origin = model.graph.addPoint({-straightLen * 0.5, 0.0, 0.0});

    std::vector<ParametricEntityId> pts;
    uint32_t arcSegs = std::max(2u, segments / 4);
    double angleStep = std::numbers::pi / static_cast<double>(arcSegs);

    for (uint32_t i = 0; i <= arcSegs; ++i) {
        double angle = -std::numbers::pi / 2.0 + static_cast<double>(i) * angleStep;
        double x = straightLen * 0.5 + radius * std::cos(angle);
        double y = radius * std::sin(angle);
        auto pt = model.graph.addPoint({x, y, 0.0});
        pts.push_back(pt);
        if (i > 0) {
            double prevAngle = -std::numbers::pi / 2.0 + static_cast<double>(i - 1) * angleStep;
            double dx = radius * (std::cos(angle) - std::cos(prevAngle));
            double dy = radius * (std::sin(angle) - std::sin(prevAngle));
            (void)model.graph.addDistanceConstraint(pts[pts.size() - 2], pt, std::sqrt(dx * dx + dy * dy));
        }
    }

    model.xHandle = pts.front();
    model.yHandle = pts.back();
    model.corner = pts[pts.size() / 2];

    return model;
}

ParametricSolverReport ParametricSampleGenerator::solveSketchRectangle(SketchSampleModel& model,
                                                                       const ParametricSolverConfig& config) noexcept {
    if (config.maxIterations == 0u) {
        ParametricSolverReport report{};
        report.converged = false;
        report.errors.push_back("maxIterations must be greater than zero");
        return report;
    }
    return ParametricSolver::solve(model.graph, config);
}

ParametricSolverReport ParametricSampleGenerator::solveSketchShape(SketchSampleModel& model,
                                                                    const ParametricSolverConfig& config) noexcept {
    return ParametricSolver::solve(model.graph, config);
}

} // namespace nexus::parametric
