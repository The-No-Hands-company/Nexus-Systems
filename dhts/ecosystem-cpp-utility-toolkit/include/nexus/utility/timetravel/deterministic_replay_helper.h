#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace nexus::utility::timetravel {

/**
 * @brief Record inputs during a run and replay them deterministically.
 */
class DeterministicReplayHelper {
public:
    enum class Mode { Idle, Recording, Replaying };

    struct InputEvent {
        std::string channel;
        std::string value;
    };

    static DeterministicReplayHelper& instance() {
        static DeterministicReplayHelper inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void startRecording() {
        std::lock_guard<std::mutex> lk(mutex_);
        mode_ = Mode::Recording;
        events_.clear();
        replayCursor_ = 0;
    }

    void startReplay() {
        std::lock_guard<std::mutex> lk(mutex_);
        mode_ = Mode::Replaying;
        replayCursor_ = 0;
    }

    void stop() {
        std::lock_guard<std::mutex> lk(mutex_);
        mode_ = Mode::Idle;
    }

    Mode mode() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return mode_;
    }

    /// During recording, capture an input; during replay, this is ignored.
    void record(const std::string& channel, const std::string& value) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (mode_ == Mode::Recording) events_.push_back({channel, value});
    }

    /// During replay, fetch the next recorded input for a channel.
    bool next(const std::string& channel, std::string& out) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (mode_ != Mode::Replaying) return false;
        while (replayCursor_ < events_.size()) {
            const auto& e = events_[replayCursor_++];
            if (e.channel == channel) { out = e.value; return true; }
        }
        return false;
    }

    std::size_t eventCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return events_.size();
    }

    /// True if replay consumed all recorded events.
    bool replayComplete() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return mode_ == Mode::Replaying && replayCursor_ >= events_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        events_.clear();
        replayCursor_ = 0;
        mode_ = Mode::Idle;
    }

private:
    DeterministicReplayHelper() = default;
    ~DeterministicReplayHelper() = default;
    DeterministicReplayHelper(const DeterministicReplayHelper&) = delete;
    DeterministicReplayHelper& operator=(const DeterministicReplayHelper&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    Mode mode_ = Mode::Idle;
    std::vector<InputEvent> events_;
    std::size_t replayCursor_ = 0;
};

} // namespace nexus::utility::timetravel
