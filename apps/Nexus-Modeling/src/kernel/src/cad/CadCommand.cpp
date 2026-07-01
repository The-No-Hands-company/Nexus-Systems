#include <nexus/cad/CadCommand.h>

namespace nexus::cad {

// ── AddSketchCommand ─────────────────────────────────────────────────────

AddSketchCommand::AddSketchCommand(parametric::SketchProfile sketch)
    : m_sketch(std::move(sketch))
{}

std::string AddSketchCommand::description() const
{
    return "Add Sketch";
}

bool AddSketchCommand::execute(CadDocument& doc)
{
    m_createdId = doc.history().addSketch(m_sketch);
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    m_executed = true;
    doc.history().rebuild();
    return true;
}

bool AddSketchCommand::undo(CadDocument& doc)
{
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    doc.history().removeFeature(m_createdId);
    m_executed = false;
    return true;
}

// ── AddExtrudeCommand ────────────────────────────────────────────────────

AddExtrudeCommand::AddExtrudeCommand(parametric::FeatureId sketchId,
                                       geometry::CurveExtrudeDesc desc)
    : m_sketchId(sketchId), m_desc(desc)
{}

std::string AddExtrudeCommand::description() const
{
    return "Extrude";
}

bool AddExtrudeCommand::execute(CadDocument& doc)
{
    m_createdId = doc.history().addExtrude(
        parametric::SketchProfile{}, m_desc);
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    m_executed = true;
    doc.history().setDirection(m_createdId, m_desc.direction);
    doc.history().setHeight(m_createdId, m_desc.height);
    doc.history().rebuild();
    return true;
}

bool AddExtrudeCommand::undo(CadDocument& doc)
{
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    doc.history().removeFeature(m_createdId);
    m_executed = false;
    return true;
}

// ── AddRevolveCommand ────────────────────────────────────────────────────

AddRevolveCommand::AddRevolveCommand(parametric::FeatureId sketchId,
                                       geometry::RevolveDesc desc)
    : m_sketchId(sketchId), m_desc(desc)
{}

std::string AddRevolveCommand::description() const
{
    return "Revolve";
}

bool AddRevolveCommand::execute(CadDocument& doc)
{
    m_createdId = doc.history().addRevolve(
        parametric::SketchProfile{}, m_desc);
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    m_executed = true;
    doc.history().setAxis(m_createdId, m_desc.axisOrigin, m_desc.axisDirection);
    doc.history().setCapEnds(m_createdId, m_desc.capEnds);
    doc.history().rebuild();
    return true;
}

bool AddRevolveCommand::undo(CadDocument& doc)
{
    if (m_createdId == parametric::kInvalidFeatureId) return false;
    doc.history().removeFeature(m_createdId);
    m_executed = false;
    return true;
}

// ── SetHeightCommand ─────────────────────────────────────────────────────

SetHeightCommand::SetHeightCommand(parametric::FeatureId id,
                                     float oldHeight, float newHeight)
    : m_featureId(id), m_oldHeight(oldHeight), m_newHeight(newHeight)
{}

std::string SetHeightCommand::description() const
{
    return "Set Height";
}

bool SetHeightCommand::execute(CadDocument& doc)
{
    if (!doc.history().setHeight(m_featureId, m_newHeight)) return false;
    m_executed = true;
    doc.history().rebuild();
    return true;
}

bool SetHeightCommand::undo(CadDocument& doc)
{
    if (!m_executed) return false;
    doc.history().setHeight(m_featureId, m_oldHeight);
    doc.history().rebuild();
    m_executed = false;
    return true;
}

// ── TransformCommand ─────────────────────────────────────────────────────────

bool TransformCommand::execute(CadDocument& doc)
{
    auto* node = doc.history().node(m_featureId);
    if(!node || !node->mesh) return false;

    if (!m_executed) {
        // First execution: store current state, leave mesh as-is
        m_newMesh = *node->mesh;
        m_executed = true;
    } else {
        // Redo after undo: restore the new state
        m_savedMesh = *node->mesh;  // save current (which is the old state after undo)
        node->mesh.emplace(m_newMesh);
    }
    return true;
}

bool TransformCommand::undo(CadDocument& doc)
{
    if(!m_executed) return false;
    auto* node = doc.history().node(m_featureId);
    if(!node || !m_savedMesh.isValid()) return false;
    m_newMesh = *node->mesh;            // save current for redo
    node->mesh.emplace(std::move(m_savedMesh)); // restore original
    m_savedMesh = nexus::geometry::Mesh{};               // consumed
    return true;
}

// ── DeleteFeatureCommand ─────────────────────────────────────────────────────

bool DeleteFeatureCommand::execute(CadDocument& doc)
{
    if(m_executed) return false;
    auto* node = doc.history().node(m_featureId);
    if(!node || node->deleted) return false;
    m_executed = true;
    // Save mesh data for potential undo.
    if(node->mesh) m_savedMesh = *node->mesh;
    node->deleted = true;
    node->mesh.reset();
    node->surf.reset();
    node->dirty = false;
    return true;
}

bool DeleteFeatureCommand::undo(CadDocument& doc)
{
    if(!m_executed) return false;
    auto* node = doc.history().node(m_featureId);
    if(!node) return false;
    node->deleted = false;
    if(m_savedMesh.has_value()) node->mesh.emplace(std::move(*m_savedMesh));
    m_executed = false;
    return true;
}

} // namespace nexus::cad
