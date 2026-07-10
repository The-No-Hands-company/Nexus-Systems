// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Debug — Selection Tracer Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/debug/SelectionTracer.h>

#include <algorithm>

namespace nexus::debug {

SelectionTracer& SelectionTracer::instance() {
    static SelectionTracer t;
    return t;
}

void SelectionTracer::record(SelectionTraceEvent event) {
    if(!m_enabled) return;
    if(m_log.size() > 500) m_log.erase(m_log.begin(), m_log.begin() + 100);
    m_log.push_back(std::move(event));
}

bool SelectionTracer::wasFeatureHighlighted(uint32_t fid) const {
    for(const auto& e : m_log) {
        if(e.type == SelectionTraceEvent::Type::HighlightRender) {
            if(std::find(e.highlightedFeatures.begin(), e.highlightedFeatures.end(), fid)
               != e.highlightedFeatures.end()) return true;
        }
    }
    return false;
}

SelectionTraceEvent SelectionTracer::lastPickEvent() const {
    for(auto it = m_log.rbegin(); it != m_log.rend(); ++it)
        if(it->type == SelectionTraceEvent::Type::ClickPick ||
           it->type == SelectionTraceEvent::Type::ClickMiss)
            return *it;
    return {};
}

// ── FrameValidator ──────────────────────────────────────────────────────────

FrameValidator& FrameValidator::instance() {
    static FrameValidator v;
    return v;
}

void FrameValidator::beginFrame(const std::vector<uint32_t>& selectedIds) {
    if(!m_enabled) return;
    m_expectedIds = selectedIds;
    std::sort(m_expectedIds.begin(), m_expectedIds.end());
    m_highlighted.clear();
    m_report = {};
}

void FrameValidator::registerHighlight(uint32_t featureId) {
    if(!m_enabled) return;
    m_highlighted.push_back(featureId);
}

void FrameValidator::endFrame() {
    if(!m_enabled) return;
    std::sort(m_highlighted.begin(), m_highlighted.end());
    m_highlighted.erase(std::unique(m_highlighted.begin(), m_highlighted.end()), m_highlighted.end());

    m_report.shouldBeHighlighted = m_expectedIds;
    m_report.actuallyHighlighted = m_highlighted;

    std::set_difference(m_expectedIds.begin(), m_expectedIds.end(),
                        m_highlighted.begin(), m_highlighted.end(),
                        std::back_inserter(m_report.missing));
    std::set_difference(m_highlighted.begin(), m_highlighted.end(),
                        m_expectedIds.begin(), m_expectedIds.end(),
                        std::back_inserter(m_report.extra));

    m_report.consistent = m_report.missing.empty() && m_report.extra.empty();
    if(!m_report.consistent) {
        m_report.message = "HIGHLIGHT MISMATCH: ";
        if(!m_report.missing.empty()) m_report.message += "missing=" + std::to_string(m_report.missing.size()) + " ";
        if(!m_report.extra.empty()) m_report.message += "extra=" + std::to_string(m_report.extra.size());
    }
}

} // namespace nexus::debug
