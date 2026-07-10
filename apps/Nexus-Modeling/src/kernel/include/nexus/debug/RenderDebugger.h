#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Debug — GPU Pipeline & Render Debugger
//
//  ShaderCompilerErrorParser  — glslang error → human-readable report
//  PipelineStateDumper         — full Vulkan pipeline state → text dump
//  FramebufferValidator        — verify expected objects rendered
//  RenderComparator            — A/B compare two render passes
//  SceneTelemetry              — per-frame stats (draw calls, tris, visibility)
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::debug {

// ── Shader compiler error parser ────────────────────────────────────────────

struct ShaderError {
    uint32_t line = 0, column = 0;
    std::string severity;  // "ERROR", "WARNING"
    std::string message;
    std::string sourceFile;
    std::string sourceLine;
};

struct ShaderCompileReport {
    bool valid = false;
    std::string stage;  // "vertex", "fragment", "compute", "mesh"
    std::vector<ShaderError> errors;
    std::vector<ShaderError> warnings;
    std::string rawOutput;
};

class ShaderCompilerErrorParser {
public:
    // Parse glslang/shaderc error output into structured errors
    [[nodiscard]] static ShaderCompileReport parse(const std::string& compilerOutput,
                                                     const std::string& stage = "");

    // Parse multiple stages from a combined log
    [[nodiscard]] static std::vector<ShaderCompileReport> parseMultiStage(
        const std::string& combinedOutput);

    [[nodiscard]] static std::string formatReport(const ShaderCompileReport& report);
};

// ── Pipeline state dumper ───────────────────────────────────────────────────

struct PipelineStateDump {
    std::string pipelineName;
    struct StageState {
        std::string stage;
        std::string entryPoint;
        bool valid = false;
    };
    std::vector<StageState> stages;
    std::string topology;
    std::string polygonMode;
    std::string cullMode;
    std::string frontFace;
    uint32_t viewportCount = 0;
    uint32_t scissorCount = 0;
    uint32_t colorAttachmentCount = 0;
    bool depthTest = false;
    bool depthWrite = false;
    bool blendEnable = false;
    bool valid = false;
};

class PipelineStateDumper {
public:
    [[nodiscard]] static std::string dump(const PipelineStateDump& state);
    [[nodiscard]] static PipelineStateDump capture(
        const std::string& pipelineName);
};

// ── Framebuffer validator ───────────────────────────────────────────────────

enum class FramebufferIssueCategory {
    Unknown,
    PossibleCullingIssue,        // Many consecutive missing objects suggest frustum culling or LOD
    PossibleTransformIssue,      // Pattern in missing/unexpected objects suggests incorrect transform
    PossibleVisibilityIssue,     // Objects present but maybe clipped or obscured
    PossibleMaterialIssue,       // If we had color data, we could detect wrong material
    PossibleIncorrectBinding,    // Unexpected objects might be from wrong shader/resource binding
    PossibleSceneMismatch        // Unexpected objects suggest wrong scene or objects
};

struct ClassifiedFramebufferIssue {
    FramebufferIssueCategory category;
    std::string description;
    // We could add more fields like confidence, affected object ID range, etc.
};

struct FramebufferExpectation {
    uint32_t expectedObjectCount = 0;
    std::vector<uint32_t> expectedObjectIds;
    float minCoveragePercent = 1.0f;
};

struct FramebufferValidationReport {
    bool valid = false;
    uint32_t totalPixels = 0;
    uint32_t renderedPixels = 0;
    uint32_t missedObjects = 0;
    std::vector<uint32_t> missingObjects;
    std::vector<uint32_t> unexpectedObjects;
    std::string message;
    // New: classified issues based on patterns
    std::vector<ClassifiedFramebufferIssue> classifiedIssues;
};

class FramebufferValidator {
public:
    [[nodiscard]] static FramebufferValidationReport validate(
        const std::vector<uint32_t>& objectIdBuffer,
        uint32_t width, uint32_t height,
        const FramebufferExpectation& expected);

    // Generate a color-coded debug image highlighting misses
    [[nodiscard]] static std::vector<uint32_t> generateDebugOverlay(
        const std::vector<uint32_t>& objectIdBuffer,
        uint32_t width, uint32_t height,
        const FramebufferValidationReport& report);
};

// ── Render comparator (A/B regression) ──────────────────────────────────────

enum class RenderDiffCategory {
    Unknown,
    ColorShift,          // Consistent color shift across region
    PositionShift,       // Pattern suggests a translation/rotation/scale
    BlurDifference,      // One image is blurrier than the other
    NoiseDifference      // One image has more noise
};

struct ClassifiedRenderDiff {
    RenderDiffCategory category;
    std::string description;
};

struct RenderDiffReport {
    uint32_t totalPixels = 0;
    uint32_t differentPixels = 0;
    float percentChanged = 0.0f;
    bool identical = true;
    std::vector<uint32_t> diffPixelOffsets;  // up to 100 worst offenders
    std::string message;
    // New: classified differences based on patterns
    std::vector<ClassifiedRenderDiff> classifiedDifferences;
};

class RenderComparator {
public:
    // Compare two RGBA8 framebuffers pixel-by-pixel
    [[nodiscard]] static RenderDiffReport compare(
        const std::vector<uint8_t>& baseline,
        const std::vector<uint8_t>& current,
        uint32_t width, uint32_t height, uint32_t channels = 4,
        uint8_t tolerance = 0);

    // Generate a heat-map diff image
    [[nodiscard]] static std::vector<uint8_t> generateDiffImage(
        const std::vector<uint8_t>& baseline,
        const std::vector<uint8_t>& current,
        uint32_t width, uint32_t height, uint32_t channels = 4);
};

// ── Scene telemetry ─────────────────────────────────────────────────────────

struct SceneTelemetrySnapshot {
    float frameTime = 0;
    uint32_t drawCalls = 0;
    uint32_t trianglesDrawn = 0;
    uint32_t objectsVisible = 0;
    uint32_t objectsTotal = 0;
    uint32_t shaderSwitches = 0;
    uint32_t textureBinds = 0;
    uint32_t bufferBinds = 0;
    uint64_t gpuMemoryUsed = 0;
    std::string dominantShader;
};

class SceneTelemetry {
public:
    static SceneTelemetry& instance();

    void capture(const SceneTelemetrySnapshot& snapshot);
    [[nodiscard]] const std::vector<SceneTelemetrySnapshot>& history() const noexcept { return m_history; }
    [[nodiscard]] SceneTelemetrySnapshot last() const noexcept;
    [[nodiscard]] float averageFrameTime(size_t window = 60) const noexcept;
    [[nodiscard]] uint32_t averageDrawCalls(size_t window = 60) const noexcept;

    void clear() noexcept { m_history.clear(); }

private:
    SceneTelemetry() = default;
    std::vector<SceneTelemetrySnapshot> m_history;
};

} // namespace nexus::debug
