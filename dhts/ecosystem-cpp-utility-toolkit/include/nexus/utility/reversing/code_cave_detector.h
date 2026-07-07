#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::reversing {

/**
 * @brief Detect code caves (contiguous empty/padding regions) in binary sections.
 *
 * Code caves are runs of null/padding bytes usable for code injection.
 */
class CodeCaveDetector {
public:
    struct CodeCave {
        std::uint64_t offset = 0;
        std::size_t size = 0;
    };

    static CodeCaveDetector& instance() {
        static CodeCaveDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setMinCaveSize(std::size_t minSize) {
        std::lock_guard<std::mutex> lk(mutex_);
        minSize_ = minSize;
    }

    /// Scan a section buffer for runs of `fillByte` at least minSize long.
    std::vector<CodeCave> scan(const std::uint8_t* data, std::size_t size,
                               std::uint64_t baseOffset = 0,
                               std::uint8_t fillByte = 0x00) {
        std::size_t minSize;
        { std::lock_guard<std::mutex> lk(mutex_); minSize = minSize_; }
        std::vector<CodeCave> caves;
        std::size_t runStart = 0;
        bool inRun = false;
        for (std::size_t i = 0; i < size; ++i) {
            if (data[i] == fillByte) {
                if (!inRun) { runStart = i; inRun = true; }
            } else if (inRun) {
                if (i - runStart >= minSize)
                    caves.push_back({baseOffset + runStart, i - runStart});
                inRun = false;
            }
        }
        if (inRun && size - runStart >= minSize)
            caves.push_back({baseOffset + runStart, size - runStart});
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& c : caves) found_.push_back(c);
        return caves;
    }

    std::vector<CodeCave> allFound() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return found_;
    }

    std::size_t largestCaveSize() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t max = 0;
        for (const auto& c : found_) if (c.size > max) max = c.size;
        return max;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        found_.clear();
    }

private:
    CodeCaveDetector() = default;
    ~CodeCaveDetector() = default;
    CodeCaveDetector(const CodeCaveDetector&) = delete;
    CodeCaveDetector& operator=(const CodeCaveDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t minSize_ = 16;
    std::vector<CodeCave> found_;
};

} // namespace nexus::utility::reversing
