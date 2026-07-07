#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <ostream>
#include <unordered_map>

namespace nexus::utility::replay {

/// @brief Record execution for replay debugging
class ExecutionRecorder {
public:
    struct Event {
        std::chrono::steady_clock::time_point timestamp;
        std::string function_name;
        std::string event_type;
        std::string data;
    };

    void startRecording();
    void stopRecording();
    bool isRecording() const;

    void recordEvent(const std::string& function_name,
                    const std::string& event_type,
                    const std::string& data = "");

    const std::vector<Event>& getEvents() const;
    void saveToFile(const std::string& filename) const;
    void loadFromFile(const std::string& filename);

    void clear();

private:
    bool recording_ = false;
    std::vector<Event> events_;
    std::chrono::steady_clock::time_point recording_start_;
};

/// @brief Checkpoint manager for saving/restoring state
template<typename StateType>
class CheckpointManager {
public:
    using SerializeFunc = std::function<std::string(const StateType&)>;
    using DeserializeFunc = std::function<StateType(const std::string&)>;

    CheckpointManager(SerializeFunc serialize, DeserializeFunc deserialize)
        : serialize_(serialize), deserialize_(deserialize)
    {}

    void saveCheckpoint(const std::string& name, const StateType& state);
    bool restoreCheckpoint(const std::string& name, StateType& state_out);

    std::vector<std::string> listCheckpoints() const;
    void deleteCheckpoint(const std::string& name);
    void clearAll();

private:
    SerializeFunc serialize_;
    DeserializeFunc deserialize_;
    std::unordered_map<std::string, std::string> checkpoints_;
};

/// @brief Ring buffer for recent function calls
class CallHistoryBuffer {
public:
    struct CallRecord {
        std::chrono::steady_clock::time_point timestamp;
        std::string function_name;
        std::vector<std::string> arguments;
        std::string return_value;
    };

    explicit CallHistoryBuffer(size_t max_size = 1000);

    void recordCall(const std::string& function_name,
                   const std::vector<std::string>& arguments = {},
                   const std::string& return_value = "");

    std::vector<CallRecord> getRecentCalls(size_t count = 100) const;
    void printHistory(std::ostream& os, size_t count = 50) const;

    void clear();

private:
    size_t max_size_;
    size_t write_index_ = 0;
    std::vector<CallRecord> buffer_;
};

// Template implementations

template<typename StateType>
void CheckpointManager<StateType>::saveCheckpoint(const std::string& name,
                                                 const StateType& state) {
    checkpoints_[name] = serialize_(state);
}

template<typename StateType>
bool CheckpointManager<StateType>::restoreCheckpoint(const std::string& name,
                                                     StateType& state_out) {
    auto it = checkpoints_.find(name);
    if (it == checkpoints_.end()) {
        return false;
    }

    state_out = deserialize_(it->second);
    return true;
}

template<typename StateType>
std::vector<std::string> CheckpointManager<StateType>::listCheckpoints() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : checkpoints_) {
        names.push_back(name);
    }
    return names;
}

template<typename StateType>
void CheckpointManager<StateType>::deleteCheckpoint(const std::string& name) {
    checkpoints_.erase(name);
}

template<typename StateType>
void CheckpointManager<StateType>::clearAll() {
    checkpoints_.clear();
}

} // namespace nexus::utility::replay
