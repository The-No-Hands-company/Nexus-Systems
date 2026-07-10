// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Debug — Input Replay Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/debug/InputReplay.h>

#include <cstdio>
#include <fstream>

namespace nexus::debug {

InputReplay& InputReplay::instance() {
    static InputReplay r;
    return r;
}

void InputReplay::startRecording() { m_recording = true; m_log.clear(); m_replayPos = 0; }
void InputReplay::stopRecording() { m_recording = false; }

void InputReplay::record(const RecordedEvent& evt) {
    if(!m_recording) return;
    if(m_log.size() > 10000) return;
    m_log.push_back(evt);
}

bool InputReplay::startReplay() {
    if(m_log.empty()) return false;
    m_replayPos = 0;
    m_replaying = true;
    m_replayStart = 0;
    return true;
}

bool InputReplay::pollReplayEvent(RecordedEvent& out) {
    if(!m_replaying || m_replayPos >= m_log.size()) {
        m_replaying = false;
        return false;
    }
    out = m_log[m_replayPos++];
    return true;
}

void InputReplay::save(const std::string& path) const {
    FILE* f = fopen(path.c_str(), "wb");
    if(!f) return;
    uint32_t count = static_cast<uint32_t>(m_log.size());
    fwrite(&count, sizeof(count), 1, f);
    fwrite(m_log.data(), sizeof(RecordedEvent), count, f);
    fclose(f);
}

void InputReplay::load(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if(!f) return;
    uint32_t count = 0;
    fread(&count, sizeof(count), 1, f);
    m_log.resize(count);
    fread(m_log.data(), sizeof(RecordedEvent), count, f);
    fclose(f);
}

} // namespace nexus::debug
