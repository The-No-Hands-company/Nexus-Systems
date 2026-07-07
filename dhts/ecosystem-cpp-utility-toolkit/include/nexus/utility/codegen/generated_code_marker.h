#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <utility>

namespace nexus::utility::codegen {

/// @brief Mark regions of files as machine-generated and query membership.
class GeneratedCodeMarker {
public:
    using Region = std::pair<int, int>; // [start_line, end_line]

    static GeneratedCodeMarker& instance() {
        static GeneratedCodeMarker inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        regions_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void markGenerated(const std::string& file, int start_line, int end_line) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (end_line < start_line) std::swap(start_line, end_line);
        regions_[file].emplace_back(start_line, end_line);
    }

    /// True if the given line falls within any generated region of the file.
    bool isGenerated(const std::string& file, int line) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = regions_.find(file);
        if (it == regions_.end()) return false;
        for (const auto& [start, end] : it->second) {
            if (line >= start && line <= end) return true;
        }
        return false;
    }

    std::vector<Region> getGeneratedRegions(const std::string& file) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = regions_.find(file);
        return it != regions_.end() ? it->second : std::vector<Region>{};
    }

private:
    GeneratedCodeMarker() = default;
    ~GeneratedCodeMarker() = default;
    GeneratedCodeMarker(const GeneratedCodeMarker&) = delete;
    GeneratedCodeMarker& operator=(const GeneratedCodeMarker&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, std::vector<Region>> regions_;
};

} // namespace nexus::utility::codegen
