#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Parametric — tiny sketch-like sample generator
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSolver.h>

namespace nexus::parametric {

struct SketchSampleModel {
    ConstraintGraph graph;
    ParametricEntityId origin = kInvalidEntityId;
    ParametricEntityId xHandle = kInvalidEntityId;
    ParametricEntityId yHandle = kInvalidEntityId;
    ParametricEntityId corner = kInvalidEntityId;
};

class ParametricSampleGenerator {
public:
    [[nodiscard]] static SketchSampleModel makeSketchRectangle(double width, double height);

    [[nodiscard]] static SketchSampleModel makeSketchCircle(double radius, uint32_t segments = 32);

    [[nodiscard]] static SketchSampleModel makeSketchArc(double radius, double startAngle, double endAngle,
                                                          uint32_t segments = 16);

    [[nodiscard]] static SketchSampleModel makeSketchSlot(double length, double width, uint32_t segments = 16);

    [[nodiscard]] static ParametricSolverReport solveSketchRectangle(SketchSampleModel& model,
                                                                      const ParametricSolverConfig& config = {}) noexcept;

    [[nodiscard]] static ParametricSolverReport solveSketchShape(SketchSampleModel& model,
                                                                  const ParametricSolverConfig& config = {}) noexcept;
};

} // namespace nexus::parametric
