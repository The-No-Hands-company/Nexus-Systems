#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Debug — Input Replay System
//
//  Records mouse/keyboard events for deterministic regression testing.
//  Replay mode feeds recorded events back through the same callbacks.
//
//  Usage: F4 = start/stop recording, Shift+F4 = replay last recording.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::debug {

struct RecordedEvent {
    enum class Type : uint8_t { KeyPress, MousePress, MouseRelease, MouseMove, Scroll };
    Type type = Type::KeyPress;
    double timestamp = 0;
    // Key events
    int key = 0, scancode = 0, action = 0, mods = 0;
    // Mouse events
    double x = 0, y = 0;
    int button = 0;
    double scrollX = 0, scrollY = 0;
};

class InputReplay {
public:
    static InputReplay& instance();

    void startRecording();
    void stopRecording();
    [[nodiscard]] bool isRecording() const noexcept { return m_recording; }

    void record(const RecordedEvent& evt);
    [[nodiscard]] const std::vector<RecordedEvent>& recording() const noexcept { return m_log; }

    // Replay feeds events through callback; returns true while events remain
    bool startReplay();
    [[nodiscard]] bool isReplaying() const noexcept { return m_replaying; }
    bool pollReplayEvent(RecordedEvent& out);  // returns false when replay done

    void save(const std::string& path) const;
    void load(const std::string& path);

    void clear() noexcept { m_log.clear(); m_recording = m_replaying = false; }

    [[nodiscard]] size_t eventCount() const noexcept { return m_log.size(); }

private:
    InputReplay() = default;
    std::vector<RecordedEvent> m_log;
    size_t m_replayPos = 0;
    double m_replayStart = 0;
    bool m_recording = false;
    bool m_replaying = false;
};

} // namespace nexus::debug
