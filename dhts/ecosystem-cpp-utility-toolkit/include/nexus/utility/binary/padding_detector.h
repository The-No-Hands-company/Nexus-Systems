#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::binary {

/**
 * @brief Detect implicit padding inserted between struct fields.
 */
class PaddingDetector {
public:
    struct Field {
        std::string name;
        std::size_t offset = 0;
        std::size_t size = 0;
        std::size_t alignment = 0;
    };

    struct PaddingGap {
        std::string afterField;
        std::size_t offset = 0;
        std::size_t bytes = 0;
    };

    static PaddingDetector& instance() {
        static PaddingDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void addField(const std::string& name, std::size_t offset, std::size_t size,
                  std::size_t alignment = 0) {
        std::lock_guard<std::mutex> lk(mutex_);
        fields_.push_back({name, offset, size, alignment});
    }

    /// Compute padding gaps between consecutive fields plus trailing padding.
    std::vector<PaddingGap> detectPadding(std::size_t structSize = 0) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<PaddingGap> gaps;
        for (std::size_t i = 0; i + 1 < fields_.size(); ++i) {
            std::size_t end = fields_[i].offset + fields_[i].size;
            std::size_t nextStart = fields_[i + 1].offset;
            if (nextStart > end)
                gaps.push_back({fields_[i].name, end, nextStart - end});
        }
        if (structSize > 0 && !fields_.empty()) {
            std::size_t end = fields_.back().offset + fields_.back().size;
            if (structSize > end)
                gaps.push_back({fields_.back().name, end, structSize - end});
        }
        return gaps;
    }

    std::size_t totalPadding(std::size_t structSize = 0) const {
        std::size_t total = 0;
        for (const auto& g : detectPadding(structSize)) total += g.bytes;
        return total;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        fields_.clear();
    }

private:
    PaddingDetector() = default;
    ~PaddingDetector() = default;
    PaddingDetector(const PaddingDetector&) = delete;
    PaddingDetector& operator=(const PaddingDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Field> fields_;
};

} // namespace nexus::utility::binary
