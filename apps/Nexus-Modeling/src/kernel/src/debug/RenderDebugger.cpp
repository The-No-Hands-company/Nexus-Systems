// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Debug — GPU Pipeline & Render Debugger Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/debug/RenderDebugger.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace nexus::debug {

// Helper function to classify issues based on patterns of missing/unexpected object IDs
std::vector<ClassifiedFramebufferIssue> classifyFramebufferIssues(
    const std::vector<uint32_t>& missing,
    const std::vector<uint32_t>& unexpected) {
    std::vector<ClassifiedFramebufferIssue> issues;

    if (missing.empty() && unexpected.empty()) {
        // No issues
        return issues;
    }

    // Sort the vectors to analyze ranges
    std::vector<uint32_t> sortedMissing = missing;
    std::vector<uint32_t> sortedUnexpected = unexpected;
    std::sort(sortedMissing.begin(), sortedMissing.end());
    std::sort(sortedUnexpected.begin(), sortedUnexpected.end());

    // Check for consecutive missing objects (possible culling or LOD issue)
    if (!sortedMissing.empty()) {
        bool allConsecutive = true;
        for (size_t i = 1; i < sortedMissing.size(); ++i) {
            if (sortedMissing[i] != sortedMissing[i-1] + 1) {
                allConsecutive = false;
                break;
            }
        }
        if (allConsecutive && sortedMissing.size() > 5) { // arbitrary threshold
            issues.push_back({FramebufferIssueCategory::PossibleCullingIssue,
                              "A block of " + std::to_string(sortedMissing.size()) +
                              " consecutive object IDs are missing, suggesting possible frustum culling or LOD issue."});
        } else {
            issues.push_back({FramebufferIssueCategory::Unknown,
                              std::to_string(sortedMissing.size()) +
                              " objects are missing (non-consecutive pattern)."});
        }
    }

    // Check for unexpected objects
    if (!sortedUnexpected.empty()) {
        bool allConsecutive = true;
        for (size_t i = 1; i < sortedUnexpected.size(); ++i) {
            if (sortedUnexpected[i] != sortedUnexpected[i-1] + 1) {
                allConsecutive = false;
                break;
            }
        }
        if (allConsecutive && sortedUnexpected.size() > 5) {
            issues.push_back({FramebufferIssueCategory::PossibleIncorrectBinding,
                              "A block of " + std::to_string(sortedUnexpected.size()) +
                              " consecutive unexpected object IDs were found, suggesting possible incorrect resource binding or wrong object draw."});
        } else {
            issues.push_back({FramebufferIssueCategory::Unknown,
                              std::to_string(sortedUnexpected.size()) +
                              " unexpected objects were found in the framebuffer (non-consecutive pattern)."});
        }
    }

    // If we have both missing and unexpected, it might be a scene mismatch or transform issue.
    if (!missing.empty() && !unexpected.empty()) {
        issues.push_back({FramebufferIssueCategory::PossibleSceneMismatch,
                          "Both missing and unexpected objects detected, which may indicate a scene mismatch or significant transform error."});
    }

    return issues;
}

// ── ShaderCompilerErrorParser ───────────────────────────────────────────────

ShaderCompileReport ShaderCompilerErrorParser::parse(const std::string& output,
                                                       const std::string& stage) {
    ShaderCompileReport report;
    report.stage = stage;
    report.rawOutput = output;

    std::istringstream stream(output);
    std::string line;
    while(std::getline(stream, line)) {
        // Parse patterns like: "ERROR: 0:42: 'foo' : undeclared identifier"
        ShaderError err;
        bool isError = line.find("ERROR:") != std::string::npos ||
                       line.find("error:") != std::string::npos;
        bool isWarn  = line.find("WARNING:") != std::string::npos ||
                       line.find("warning:") != std::string::npos;

        if(isError || isWarn) {
            err.message = line;
            err.severity = isError ? "ERROR" : "WARNING";
            if(isError) report.errors.push_back(err);
            else report.warnings.push_back(err);
        }
    }
    report.valid = true;
    return report;
}

std::vector<ShaderCompileReport> ShaderCompilerErrorParser::parseMultiStage(
    const std::string& output) {
    std::vector<ShaderCompileReport> reports;
    std::istringstream stream(output);
    std::string line, currentStage = "unknown", currentOutput;
    while(std::getline(stream, line)) {
        if(line.find("Compiling") != std::string::npos) {
            if(!currentOutput.empty()) {
                reports.push_back(parse(currentOutput, currentStage));
                currentOutput.clear();
            }
            currentStage = line;
        } else {
            currentOutput += line + '\n';
        }
    }
    if(!currentOutput.empty())
        reports.push_back(parse(currentOutput, currentStage));
    return reports;
}

std::string ShaderCompilerErrorParser::formatReport(const ShaderCompileReport& report) {
    std::ostringstream oss;
    oss << "[" << report.stage << "] ";
    if(report.errors.empty() && report.warnings.empty()) {
        oss << "OK";
    } else {
        oss << report.errors.size() << " errors, " << report.warnings.size() << " warnings";
        for(const auto& e : report.errors)
            oss << "\n  " << e.message;
        for(const auto& w : report.warnings)
            oss << "\n  " << w.message;
    }
    return oss.str();
}

// ── PipelineStateDumper ───────────────────────────────────────────────────────

std::string PipelineStateDumper::dump(const PipelineStateDump& state) {
    std::ostringstream oss;
    oss << "Pipeline: " << state.pipelineName << "\n";
    oss << "  Topology: " << state.topology << "\n";
    oss << "  PolyMode: " << state.polygonMode << " Cull: " << state.cullMode
        << " FrontFace: " << state.frontFace << "\n";
    oss << "  Viewports: " << state.viewportCount
        << " Scissors: " << state.scissorCount << "\n";
    oss << "  ColorAttachments: " << state.colorAttachmentCount << "\n";
    oss << "  Depth: test=" << state.depthTest << " write=" << state.depthWrite
        << " Blend=" << state.blendEnable << "\n";
    for(const auto& s : state.stages)
        oss << "  Stage[" << s.stage << "]='" << s.entryPoint
            << "' valid=" << s.valid << "\n";
    return oss.str();
}

PipelineStateDump PipelineStateDumper::capture(const std::string& name) {
    PipelineStateDump d;
    d.pipelineName = name;
    d.valid = false; // Requires actual Vulkan device handle — placeholder for now
    return d;
}

// ── FramebufferValidator ─────────────────────────────────────────────────────

FramebufferValidationReport FramebufferValidator::validate(
    const std::vector<uint32_t>& idBuf, uint32_t w, uint32_t h,
    const FramebufferExpectation& expected) {
    FramebufferValidationReport report;
    report.totalPixels = w * h;
    report.renderedPixels = 0;
    std::unordered_set<uint32_t> seen, expectedSet(expected.expectedObjectIds.begin(),
                                                     expected.expectedObjectIds.end());

    for(uint32_t px : idBuf) {
        if(px != ~0u) { 
            report.renderedPixels++; 
            seen.insert(px); 
        }
    }

    for(uint32_t id : expected.expectedObjectIds)
        if(!seen.count(id)) 
            report.missingObjects.push_back(id);
    for(uint32_t id : seen)
        if(!expectedSet.count(id)) 
            report.unexpectedObjects.push_back(id);

    report.missedObjects = static_cast<uint32_t>(report.missingObjects.size());
    report.valid = report.missingObjects.empty();

    if(!report.valid) {
        std::ostringstream oss;
        oss << "Missing " << report.missingObjects.size() << " objects";
        if(!report.unexpectedObjects.empty())
            oss << ", " << report.unexpectedObjects.size() << " unexpected";
        report.message = oss.str();

        // Classify the issues
        report.classifiedIssues = classifyFramebufferIssues(
            report.missingObjects, report.unexpectedObjects);
    }
    return report;
}

std::vector<uint32_t> FramebufferValidator::generateDebugOverlay(
    const std::vector<uint32_t>& idBuf, uint32_t w, uint32_t h,
    const FramebufferValidationReport& report) {
    (void)h;
    std::vector<uint32_t> overlay(idBuf.size(), 0xFF000000);
    std::unordered_set<uint32_t> missingSet(report.missingObjects.begin(),
                                              report.missingObjects.end());
    uint32_t missColor = 0xFFFF0000; // red for missing
    for(size_t i = 0; i < idBuf.size(); ++i) {
        if(idBuf[i] != ~0u) 
            overlay[i] = idBuf[i];
        if(!report.missingObjects.empty() && i < static_cast<size_t>(w)) {
            overlay[i] = missColor; // top line = red if any misses
        }
    }
    return overlay;
}

// ── RenderComparator ─────────────────────────────────────────────────────────

RenderDiffReport RenderComparator::compare(
    const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
    uint32_t w, uint32_t h, uint32_t channels, uint8_t tolerance) {
    RenderDiffReport report;
    report.totalPixels = w * h;
    report.differentPixels = 0;
    report.identical = true;

    size_t count = std::min(a.size(), b.size());
    for(size_t i = 0; i < count; i += channels) {
        bool same = true;
        for(uint32_t c = 0; c < channels && (i + c) < count; ++c)
            if(std::abs((int)a[i+c] - (int)b[i+c]) > tolerance) { same = false; break; }
        if(!same) { 
            report.differentPixels++;
            if(report.diffPixelOffsets.size() < 100)
                report.diffPixelOffsets.push_back(static_cast<uint32_t>(i));
        }
    }

    if(report.totalPixels > 0)
        report.percentChanged = 100.0f * report.differentPixels / report.totalPixels;
    report.identical = report.differentPixels == 0;

    if(!report.identical) {
        std::ostringstream oss;
        oss << report.differentPixels << " pixels differ (" << report.percentChanged << "%)";
        report.message = oss.str();
    }
    return report;
}

std::vector<uint8_t> RenderComparator::generateDiffImage(
    const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
    uint32_t w, uint32_t h, uint32_t channels) {
    (void)w; (void)h;
    std::vector<uint8_t> diff(std::min(a.size(), b.size()), 0);
    for(size_t i = 0; i < diff.size(); i += channels) {
        bool same = true;
        for(uint32_t c = 0; c < channels && (i + c) < diff.size(); ++c)
            if(a[i+c] != b[i+c]) { same = false; break; }
        if(!same) {
            diff[i] = 255;   // R = red for diff
            if(channels > 1) diff[i+1] = 0;
            if(channels > 2) diff[i+2] = 0;
        } else {
            if(channels == 4) 
                diff[i+3] = 64; // dim identical pixels
        }
    }
    return diff;
}

// ── SceneTelemetry ───────────────────────────────────────────────────────────

SceneTelemetry& SceneTelemetry::instance() {
    static SceneTelemetry t;
    return t;
}

void SceneTelemetry::capture(const SceneTelemetrySnapshot& s) {
    if(m_history.size() > 1000) 
        m_history.erase(m_history.begin(), m_history.begin() + 200);
    m_history.push_back(s);
}

SceneTelemetrySnapshot SceneTelemetry::last() const noexcept {
    return m_history.empty() ? SceneTelemetrySnapshot{} : m_history.back();
}

float SceneTelemetry::averageFrameTime(size_t window) const noexcept {
    if(m_history.empty()) return 0;
    size_t start = m_history.size() > window ? m_history.size() - window : 0;
    float sum = 0; 
    size_t count = 0;
    for(size_t i = start; i < m_history.size(); ++i) { 
        sum += m_history[i].frameTime; 
        count++; 
    }
    return count > 0 ? sum / count : 0;
}

uint32_t SceneTelemetry::averageDrawCalls(size_t window) const noexcept {
    if(m_history.empty()) return 0;
    size_t start = m_history.size() > window ? m_history.size() - window : 0;
    uint64_t sum = 0; 
    size_t count = 0;
    for(size_t i = start; i < m_history.size(); ++i) { 
        sum += m_history[i].drawCalls; 
        count++; 
    }
    return count > 0 ? static_cast<uint32_t>(sum / count) : 0;
}

} // namespace nexus::debug
