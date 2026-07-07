#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <set>
#include <string>

namespace nexus::utility::datastructures {

/// @brief Tracks active iterators per container and validates dereferences after modification.
class IteratorDebug {
public:
    static IteratorDebug& instance() {
        static IteratorDebug inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        active_.clear();
        iter_container_.clear();
        invalid_dereferences_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void registerIterator(const std::string& container, void* iter_ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_[container].insert(iter_ptr);
        iter_container_[iter_ptr] = container;
    }

    void unregisterIterator(const std::string& container, void* iter_ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = active_.find(container);
        if (it != active_.end()) {
            it->second.erase(iter_ptr);
        }
        iter_container_.erase(iter_ptr);
    }

    /// @brief Validate an iterator dereference.
    /// @return true if safe (registered and container not modified since), false otherwise.
    bool validateDereference(void* iter_ptr, bool container_modified) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = iter_container_.find(iter_ptr);
        bool registered = (it != iter_container_.end());
        if (!registered || container_modified) {
            ++invalid_dereferences_;
            return false;
        }
        return true;
    }

    size_t getActiveCount(const std::string& container) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = active_.find(container);
        return it != active_.end() ? it->second.size() : 0;
    }

    size_t getInvalidDereferenceCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return invalid_dereferences_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        active_.clear();
        iter_container_.clear();
        invalid_dereferences_ = 0;
    }

private:
    IteratorDebug() = default;
    ~IteratorDebug() = default;
    IteratorDebug(const IteratorDebug&) = delete;
    IteratorDebug& operator=(const IteratorDebug&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::set<void*>> active_;
    std::map<void*, std::string> iter_container_;
    size_t invalid_dereferences_ = 0;
};

} // namespace nexus::utility::datastructures
