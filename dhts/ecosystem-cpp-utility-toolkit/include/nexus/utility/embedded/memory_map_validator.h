#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <cstddef>

namespace nexus::utility::embedded {

class MemoryMapValidator {
public:
    struct Region {
        std::string name;
        uintptr_t start;
        size_t size;
    };

    static MemoryMapValidator& instance() {
        static MemoryMapValidator inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void registerRegion(const std::string& name, uintptr_t start, size_t size) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return;
        regions_.push_back({name, start, size});
    }

    bool checkOverlap(uintptr_t addr, size_t size) const {
        std::lock_guard<std::mutex> lk(mutex_);
        uintptr_t end = addr + size;
        for (const auto& r : regions_) {
            uintptr_t r_end = r.start + r.size;
            if (!(end <= r.start || addr >= r_end)) return true;
        }
        return false;
    }

    size_t getRegionCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return regions_.size();
    }

private:
    MemoryMapValidator() = default;
    ~MemoryMapValidator() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::vector<Region> regions_;
};

} // namespace nexus::utility::embedded
