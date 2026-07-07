#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <mutex>
#include <atomic>

namespace nexus::utility::codequality {

/**
 * @brief Track and export per-file line coverage percentages.
 */
class CodeCoverageExporter {
public:
    struct FileCoverage {
        std::string file;
        std::size_t totalLines = 0;
        std::size_t coveredLines = 0;
        double percentage() const {
            return totalLines == 0 ? 0.0 : (100.0 * coveredLines) / totalLines;
        }
    };

    static CodeCoverageExporter& instance() {
        static CodeCoverageExporter inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordLine(const std::string& file, std::size_t line, bool covered) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& data = files_[file];
        auto [it, inserted] = data.emplace(line, covered);
        if (!inserted && covered) it->second = true;
    }

    FileCoverage coverage(const std::string& file) const {
        std::lock_guard<std::mutex> lk(mutex_);
        FileCoverage fc{file, 0, 0};
        auto it = files_.find(file);
        if (it != files_.end()) {
            fc.totalLines = it->second.size();
            for (const auto& [ln, cov] : it->second) if (cov) ++fc.coveredLines;
        }
        return fc;
    }

    double overallPercentage() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t total = 0, covered = 0;
        for (const auto& [f, lines] : files_) {
            total += lines.size();
            for (const auto& [ln, cov] : lines) if (cov) ++covered;
        }
        return total == 0 ? 0.0 : (100.0 * covered) / total;
    }

    std::string exportLcov() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::ostringstream os;
        for (const auto& [file, lines] : files_) {
            os << "SF:" << file << "\n";
            std::size_t hit = 0;
            for (const auto& [ln, cov] : lines) {
                os << "DA:" << ln << "," << (cov ? 1 : 0) << "\n";
                if (cov) ++hit;
            }
            os << "LF:" << lines.size() << "\n";
            os << "LH:" << hit << "\n";
            os << "end_of_record\n";
        }
        return os.str();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        files_.clear();
    }

private:
    CodeCoverageExporter() = default;
    ~CodeCoverageExporter() = default;
    CodeCoverageExporter(const CodeCoverageExporter&) = delete;
    CodeCoverageExporter& operator=(const CodeCoverageExporter&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, std::unordered_map<std::size_t, bool>> files_;
};

} // namespace nexus::utility::codequality
