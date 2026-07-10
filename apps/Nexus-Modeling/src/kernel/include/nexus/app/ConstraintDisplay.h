#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus App Constraint Display — Constraint Visualization in Viewport
//
//  Renders constraint glyphs (coincident dots, horizontal/vertical marks,
//  parallel/perpendicular icons, dimension arrows) during sketch mode.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSketchProfile.h>

namespace nexus::app {

class ConstraintDisplay {
public:
    static void render(const nexus::parametric::SketchProfile& profile,
                       const nexus::parametric::ConstraintGraph& graph);

private:
    static void renderCoincident(const nexus::parametric::ConstraintGraph& graph);
    static void renderHorizontal(const nexus::parametric::ConstraintGraph& graph);
    static void renderVertical(const nexus::parametric::ConstraintGraph& graph);
    static void renderDistance(const nexus::parametric::ConstraintGraph& graph);
    static void renderAngle(const nexus::parametric::ConstraintGraph& graph);
    static void renderParallel(const nexus::parametric::ConstraintGraph& graph);
    static void renderTangent(const nexus::parametric::ConstraintGraph& graph);
    static void renderMidpoint(const nexus::parametric::ConstraintGraph& graph);
};

} // namespace nexus::app
