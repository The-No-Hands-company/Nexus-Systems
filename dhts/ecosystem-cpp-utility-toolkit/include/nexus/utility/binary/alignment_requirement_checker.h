#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::binary {

/**
 * @brief Check alignment requirements for types and addresses.
 */
class AlignmentRequirementChecker {
public:
    struct FieldAlignment {
        std::string name;
        std::size_t offset = 0;
        std::size_t alignment = 1;
        bool aligned = true;
    };

    static AlignmentRequirementChecker& instance() {
        static AlignmentRequirementChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static bool isAligned(std::uint64_t address, std::size_t alignment) {
        return alignment != 0 && (address % alignment) == 0;
    }

    /// Number of padding bytes needed to align `offset` to `alignment`.
    static std::size_t paddingFor(std::size_t offset, std::size_t alignment) {
        if (alignment == 0) return 0;
        std::size_t rem = offset % alignment;
        return rem == 0 ? 0 : alignment - rem;
    }

    /// Align `offset` up to the next multiple of `alignment`.
    static std::size_t alignUp(std::size_t offset, std::size_t alignment) {
        return offset + paddingFor(offset, alignment);
    }

    /// Record and validate a field's placement.
    bool checkField(const std::string& name, std::size_t offset, std::size_t alignment) {
        bool ok = isAligned(offset, alignment);
        std::lock_guard<std::mutex> lk(mutex_);
        fields_.push_back({name, offset, alignment, ok});
        if (!ok) ++violations_;
        return ok;
    }

    std::vector<FieldAlignment> violations() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<FieldAlignment> out;
        for (const auto& f : fields_) if (!f.aligned) out.push_back(f);
        return out;
    }

    std::size_t violationCount() const { return violations_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        fields_.clear();
        violations_ = 0;
    }

private:
    AlignmentRequirementChecker() = default;
    ~AlignmentRequirementChecker() = default;
    AlignmentRequirementChecker(const AlignmentRequirementChecker&) = delete;
    AlignmentRequirementChecker& operator=(const AlignmentRequirementChecker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<FieldAlignment> fields_;
    std::atomic<std::size_t> violations_{0};
};

} // namespace nexus::utility::binary
