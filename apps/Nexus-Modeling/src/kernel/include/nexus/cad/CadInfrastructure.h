#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Design Rules, Dependency Graph, Layers, External Refs,
//              Undo Bookmarks, Sketcher Diagnostics
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/cad/CadDataTables.h>
#include <nexus/cad/CadDesignRule.h>
#include <nexus/parametric/ConstraintGraph.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus::cad {

// ──────────── Feature Dependency Graph ──────────────────────────────────────

struct DependencyEdge {
    parametric::FeatureId from;
    parametric::FeatureId to;
    enum Relation { Parent, Child, Reference, Sketch } relation = Parent;
};

class CadDependencyGraph {
public:
    // Build the dependency graph from a document.
    [[nodiscard]] static std::vector<DependencyEdge> build(
        const CadDocument& doc) noexcept;

    // Find all features that depend on a given feature (downstream).
    [[nodiscard]] static std::vector<parametric::FeatureId> downstream(
        const CadDocument& doc,
        parametric::FeatureId featureId) noexcept;

    // Find all features that a given feature depends on (upstream).
    [[nodiscard]] static std::vector<parametric::FeatureId> upstream(
        const CadDocument& doc,
        parametric::FeatureId featureId) noexcept;

    // Topological sort of features (build order).
    [[nodiscard]] static std::vector<parametric::FeatureId> buildOrder(
        const CadDocument& doc) noexcept;

    // Detect circular dependencies.
    [[nodiscard]] static bool hasCycles(const CadDocument& doc) noexcept;
};

// ──────────── Layer System ─────────────────────────────────────────────────

struct CadLayer {
    std::string name;
    uint32_t color = 0xFFFFFF;
    bool visible = true;
    bool locked = false;
    std::vector<parametric::FeatureId> features;
};

class CadLayerManager {
public:
    // Create a new layer.
    [[nodiscard]] uint32_t createLayer(const std::string& name,
                                         uint32_t color = 0xFFFFFF);

    // Assign features to a layer.
    void assignToLayer(uint32_t layerId,
                       const std::vector<parametric::FeatureId>& featureIds);

    // Toggle layer visibility.
    void setVisible(uint32_t layerId, bool visible);
    void setLocked(uint32_t layerId, bool locked);

    [[nodiscard]] const std::vector<CadLayer>& layers() const noexcept;
    [[nodiscard]] CadLayer* layer(uint32_t id) noexcept;

private:
    std::vector<CadLayer> m_layers;
};

// ──────────── Named Undo Bookmarks ──────────────────────────────────────────

struct UndoBookmark {
    std::string name;
    size_t undoStackDepth = 0;
};

class CadBookmarks {
public:
    // Save current undo position as a named bookmark.
    void save(const std::string& name, size_t stackDepth);

    // Restore to a bookmark.
    [[nodiscard]] bool restore(CadDocument& doc, const std::string& name);

    // List bookmark names.
    [[nodiscard]] std::vector<std::string> names() const noexcept;

    void clear();

private:
    std::vector<UndoBookmark> m_bookmarks;
};

// ──────────── External References ───────────────────────────────────────────

struct CadXRef {
    std::string filePath;
    std::string name;
    bool loaded = false;
    bool modified = false;
};

class CadXRefManager {
public:
    // Attach an external CAD document as a reference.
    [[nodiscard]] bool attach(const std::string& filePath,
                               const std::string& name);

    // Reload a referenced document.
    [[nodiscard]] bool reload(const std::string& name);

    // Unload a reference.
    void detach(const std::string& name);

    // Check if any references have been modified.
    [[nodiscard]] bool anyModified() const noexcept;

    [[nodiscard]] const std::vector<CadXRef>& refs() const noexcept;

private:
    std::vector<CadXRef> m_refs;
};

// ──────────── Sketcher Diagnostics ─────────────────────────────────────────

struct SketcherDiagnostic {
    parametric::ParametricEntityId entityId;
    enum Type { UnderConstrained, FullyConstrained, OverConstrained,
                DanglingEntity, UnsolvedConstraint } type;
    std::string message;
};

class CadSketcherDiagnostics {
public:
    // Analyze a constraint graph for issues.
    [[nodiscard]] static std::vector<SketcherDiagnostic> analyze(
        const parametric::ConstraintGraph& graph) noexcept;

    // Count constraints per entity (degree).
    [[nodiscard]] static std::unordered_map<parametric::ParametricEntityId, uint32_t>
    constraintDegrees(const parametric::ConstraintGraph& graph) noexcept;

    // Get the DOF analysis summary.
    [[nodiscard]] static parametric::DegreesOfFreedom dofSummary(
        const parametric::ConstraintGraph& graph) noexcept;

    // Generate a human-readable report.
    [[nodiscard]] static std::string generateReport(
        const parametric::ConstraintGraph& graph) noexcept;
};

} // namespace nexus::cad
