#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace nexus::utility::codegen {

/// @brief Debug generated code output: track emitted files and line counts.
class CodeGeneratorDebugger {
public:
    static CodeGeneratorDebugger& instance() {
        static CodeGeneratorDebugger inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        files_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// Record a chunk of code emitted to a file; counts newlines as lines.
    void recordEmit(const std::string& file, const std::string& code) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t lines = std::count(code.begin(), code.end(), '\n');
        if (!code.empty() && code.back() != '\n') ++lines;
        files_[file] += lines;
    }

    std::vector<std::string> getGeneratedFiles() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(files_.size());
        for (const auto& [name, lines] : files_) names.push_back(name);
        return names;
    }

    size_t getTotalLines() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = 0;
        for (const auto& [name, lines] : files_) total += lines;
        return total;
    }

    size_t getLinesByFile(const std::string& file) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(file);
        return it != files_.end() ? it->second : 0;
    }

private:
    CodeGeneratorDebugger() = default;
    ~CodeGeneratorDebugger() = default;
    CodeGeneratorDebugger(const CodeGeneratorDebugger&) = delete;
    CodeGeneratorDebugger& operator=(const CodeGeneratorDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, size_t> files_;
};

} // namespace nexus::utility::codegen
