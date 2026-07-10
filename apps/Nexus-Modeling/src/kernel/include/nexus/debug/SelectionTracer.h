#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Debug — Selection Tracing Utility
//
//  Traces every selection-related event with spatial context.  Answers:
//  "What did the user click on?", "Why did the highlight change (or not)?",
//  "What are the exact selectedIds at every frame?".
//
//  Usage: Press Ctrl+Shift+F2 to toggle trace overlay.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/render/Camera.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nexus::debug {

using Vec3 = nexus::render::Vec3;

struct SelectionTraceEvent {
    enum class Type : uint8_t {
        ClickPick, ClickMiss, Deselect, SubModeChange, GizmoStart, GizmoEnd,
        SelectAll, SelectNone, BoxSelect, OutlinerClick, MenuDeselect,
        HighlightRender, FrameBegin,
    };
    Type type = Type::ClickMiss;
    float timestamp = 0;
    uint32_t featureId = 0;
    uint32_t faceIndex = ~0u;
    uint32_t vertexIndex = ~0u;
    float screenX = 0, screenY = 0;
    Vec3 worldHit{};
    std::vector<uint32_t> selectedBefore;
    std::vector<uint32_t> selectedAfter;
    std::vector<uint32_t> highlightedFeatures;
    std::string note;
};

class SelectionTracer {
public:
    static SelectionTracer& instance();

    void enable(bool on = true) noexcept { m_enabled = on; }
    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

    void record(SelectionTraceEvent event);
    [[nodiscard]] const std::vector<SelectionTraceEvent>& events() const noexcept { return m_log; }

    // Quick access helpers
    [[nodiscard]] bool wasFeatureHighlighted(uint32_t fid) const;
    [[nodiscard]] SelectionTraceEvent lastPickEvent() const;

    void clear() noexcept { m_log.clear(); }

private:
    SelectionTracer() = default;
    std::vector<SelectionTraceEvent> m_log;
    bool m_enabled = false;
};

// ── Frame-by-frame highlight validator ─────────────────────────────────────

struct FrameHighlightReport {
    bool consistent = true;
    std::vector<uint32_t> shouldBeHighlighted;
    std::vector<uint32_t> actuallyHighlighted;
    std::vector<uint32_t> missing;  // in selection but not highlighted
    std::vector<uint32_t> extra;    // highlighted but not in selection
    std::string message;
};

class FrameValidator {
public:
    static FrameValidator& instance();

    void enable(bool on = true) noexcept { m_enabled = on; }
    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

    // Call beginFrame at start of render loop, endFrame at end.
    void beginFrame(const std::vector<uint32_t>& selectedIds);
    void registerHighlight(uint32_t featureId);
    void endFrame();

    [[nodiscard]] const FrameHighlightReport& lastReport() const noexcept { return m_report; }
    [[nodiscard]] bool wasConsistent() const noexcept { return m_report.consistent; }

private:
    FrameValidator() = default;
    FrameHighlightReport m_report;
    std::vector<uint32_t> m_expectedIds;
    std::vector<uint32_t> m_highlighted;
    bool m_enabled = false;
};

} // namespace nexus::debug
