#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <map>
#include <mutex>

namespace nexus::utility::gamedev {

class InputReplayRecorder {
public:
    static InputReplayRecorder& instance() {
        static InputReplayRecorder inst;
        return inst;
    }

    void initialize(const std::string& config = "") { config_ = config; enabled_ = true; }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    std::string getStatus() const { return enabled_ ? "active" : "inactive"; }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_inputs_.clear();
    }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_inputs_.clear();
    }

    void recordInput(uint64_t frame, const std::string& input_data) {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_inputs_[frame].push_back(input_data);
    }

    std::vector<std::string> getInputs(uint64_t frame) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = frame_inputs_.find(frame);
        if (it == frame_inputs_.end()) return {};
        return it->second;
    }

    uint64_t getFrameCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (frame_inputs_.empty()) return 0;
        return frame_inputs_.rbegin()->first + 1;
    }

    std::vector<std::string> playBack(uint64_t frame) const {
        return getInputs(frame);
    }

private:
    InputReplayRecorder() = default;
    ~InputReplayRecorder() = default;

    InputReplayRecorder(const InputReplayRecorder&) = delete;
    InputReplayRecorder& operator=(const InputReplayRecorder&) = delete;
    InputReplayRecorder(InputReplayRecorder&&) = delete;
    InputReplayRecorder& operator=(InputReplayRecorder&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<uint64_t, std::vector<std::string>> frame_inputs_;
};

} // namespace nexus::utility::gamedev
