#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Parametric — deterministic solver v0
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/parametric/ConstraintGraph.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::parametric {

struct ParametricSolverConfig {
    uint32_t maxIterations = 16;
    double convergenceEpsilon = 1e-8;
};

struct ParametricSolverReport {
    bool converged = true;
    uint32_t iterationsRan = 0;
    double maxConstraintError = 0.0;
    std::vector<std::string> errors;
};

class ParametricSolver {
public:
    static ParametricSolverReport solve(ConstraintGraph& graph,
                                        const ParametricSolverConfig& config = {}) noexcept;
};

} // namespace nexus::parametric
