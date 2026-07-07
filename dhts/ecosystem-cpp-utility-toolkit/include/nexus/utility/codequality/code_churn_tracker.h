#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <atomic>

namespace nexus::utility::codequality {

/**
 * @brief Track frequently changed code (churn) per file.
 */
class CodeChurnTracker {
public:
    struct FileChurn {
        std::string file;
        std::size_t commits = 0;
        std::size_t linesAdded = 0;
        std::size_t linesDeleted = 0;
        std::size_t totalChurn() const { return linesAdded + linesDeleted; }
    };

    static CodeChurnTracker& instance() {
        static CodeChurnTracker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordChange(const std::string& file, std::size_t added, std::size_t deleted) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& fc = files_[file];
        fc.file = file;
        fc.commits += 1;
        fc.linesAdded += added;
        fc.linesDeleted += deleted;
    }

    FileChurn getChurn(const std::string& file) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = files_.find(file);
        return it == files_.end() ? FileChurn{file, 0, 0, 0} : it->second;
    }

    std::size_t totalChurn() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t total = 0;
        for (const auto& [k, v] : files_) total += v.totalChurn();
        return total;
    }

    std::vector<FileChurn> mostChurned(std::size_t topN) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<FileChurn> out;
        out.reserve(files_.size());
        for (const auto& [k, v] : files_) out.push_back(v);
        std::sort(out.begin(), out.end(), [](const FileChurn& a, const FileChurn& b) {
            return a.totalChurn() > b.totalChurn();
        });
        if (out.size() > topN) out.resize(topN);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        files_.clear();
    }

private:
    CodeChurnTracker() = default;
    ~CodeChurnTracker() = default;
    CodeChurnTracker(const CodeChurnTracker&) = delete;
    CodeChurnTracker& operator=(const CodeChurnTracker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, FileChurn> files_;
};

} // namespace nexus::utility::codequality
