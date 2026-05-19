#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Scripting and Batch Pipeline v0
//
//  Thin deterministic command registry over kernel domain APIs.
//  This is intentionally small: typed command handlers, line-oriented scripts,
//  and a batch harness for headless / CI execution.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/asset/SceneAsset.h>
#include <nexus/animation/AnimationCore.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/MeshIO.h>
#include <nexus/gfx/GaussianSplatting.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSerialization.h>
#include <nexus/parametric/ParametricSolver.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/sim/ClothSolver.h>
#include <nexus/sim/FluidSolver.h>
#include <nexus/sim/SimulationCore.h>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

struct ScriptCommand {
    std::string name;
    std::map<std::string, std::string> args;
};

struct ScriptStepReport {
    size_t lineNumber = 0;
    std::string command;
    bool success = false;
    // messages is sorted lexicographically on every return path; callers may rely on this.
    std::vector<std::string> messages;
};

struct ScriptRunReport {
    bool valid = true;
    std::vector<ScriptStepReport> steps;
    // messages is sorted lexicographically on every return path; callers may rely on this.
    std::vector<std::string> messages;
};

struct ScriptContext {
    std::filesystem::path workingDirectory;

    nexus::geometry::Mesh mesh;
    bool hasMesh = false;

    nexus::asset::SceneAsset scene;
    bool hasScene = false;

    nexus::animation::Skeleton skeleton;
    nexus::animation::Pose pose;
    bool hasSkeleton = false;

    std::unique_ptr<nexus::gfx::RenderContext> renderContext;

    // ── Simulation ────────────────────────────────────────────────────────────
    std::unique_ptr<nexus::RigidBodySolver>    rigidSolver;
    nexus::SimState                            rigidLastState;
    bool                                       hasRigidSolver  = false;
    bool                                       hasRigidLastState = false;

    std::unique_ptr<nexus::ClothSolver>        clothSolver;
    nexus::ClothState                          clothLastState;
    bool                                       hasClothSolver  = false;
    bool                                       hasClothLastState = false;

    std::unique_ptr<nexus::FluidSolver>        fluidSolver;
    nexus::FluidState                          fluidLastState;
    bool                                       hasFluidSolver  = false;
    bool                                       hasFluidLastState = false;
    std::vector<uint8_t>                       crossSolverBaselineBytes;
    bool                                       hasCrossSolverBaseline = false;

    // ── Gaussian Splatting ────────────────────────────────────────────────────
    nexus::gfx::GaussianSplatCloud             gaussianCloud;
    bool                                       hasGaussianCloud = false;
    std::vector<uint8_t>                       gaussianBaselineBytes;
    bool                                       hasGaussianBaseline = false;

    // ── Parametric ────────────────────────────────────────────────────────────
    nexus::parametric::ConstraintGraph         parametricGraph;
    bool                                       hasParametricGraph = false;

    // ── Mesh baseline ─────────────────────────────────────────────────────────
    nexus::geometry::Mesh                      meshBaseline;
    bool                                       hasMeshBaseline = false;

    // ── Scene baseline ────────────────────────────────────────────────────────
    std::vector<uint8_t>                       sceneBaselineBytes;
    bool                                       hasSceneBaseline = false;

    // ── Parametric baseline ───────────────────────────────────────────────────
    std::vector<uint8_t>                       parametricBaselineBytes;
    bool                                       hasParametricBaseline = false;

    // ── Animation baseline ────────────────────────────────────────────────────
    nexus::animation::Skeleton                 skeletonBaseline;
    nexus::animation::Pose                     poseBaseline;
    bool                                       hasAnimBaseline = false;

    std::string sceneName;
    std::string meshName;
};

class ScriptRegistry {
public:
    using CommandHandler = std::function<bool(ScriptContext&, const ScriptCommand&, std::vector<std::string>&)>;

    bool registerCommand(std::string name, CommandHandler handler);
    [[nodiscard]] bool hasCommand(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> listCommands() const;
    [[nodiscard]] bool execute(ScriptContext& context,
                               const ScriptCommand& command,
                               std::vector<std::string>& messages) const;

private:
    std::map<std::string, CommandHandler> m_handlers;
};

class ScriptBatchHarness {
public:
    ScriptBatchHarness();

    [[nodiscard]] const ScriptRegistry& registry() const noexcept { return m_registry; }
    [[nodiscard]] ScriptRegistry& registry() noexcept { return m_registry; }

    [[nodiscard]] ScriptRunReport runScript(std::string_view scriptText,
                                           ScriptContext& context) const;
    [[nodiscard]] ScriptRunReport runScriptFile(const std::filesystem::path& scriptPath,
                                                ScriptContext& context) const;

    static std::string normalizePath(const std::filesystem::path& base,
                                     const std::string& pathText);

private:
    ScriptRegistry m_registry;

    static ScriptCommand parseCommandLine(std::string_view line,
                                          std::vector<std::string>& errors);
    static std::vector<std::string> splitScriptLines(std::string_view scriptText);
    static std::optional<std::string> stripQuotes(std::string_view text);
    static std::optional<float> parseFloat(std::string_view text);
    static std::optional<int32_t> parseInt(std::string_view text);
    static std::optional<bool> parseBool(std::string_view text);

    void registerBuiltinCommands();
};

} // namespace nexus::automation
