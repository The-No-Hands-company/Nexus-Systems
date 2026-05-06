#include <nexus/render/FrameCaptureExporter.h>

namespace nexus::render {

void NullFrameCaptureExporter::beginCapture(uint64_t frameIndex)
{
    m_inProgress             = FrameCaptureSnapshot{};
    m_inProgress.frameIndex  = frameIndex;
    m_capturing              = true;
}

void NullFrameCaptureExporter::recordPass(const FramePassEntry& entry)
{
    if (!m_capturing) {
        return;
    }
    m_inProgress.passes.push_back(entry);
    m_inProgress.totalDrawCalls += entry.drawCalls;
    m_inProgress.totalTriangles += entry.triangles;
}

void NullFrameCaptureExporter::endCapture()
{
    if (!m_capturing) {
        return;
    }
    m_completed    = std::move(m_inProgress);
    m_inProgress   = {};
    m_capturing    = false;
    m_hasSnapshot  = true;
    ++m_totalFrames;
}

} // namespace nexus::render
