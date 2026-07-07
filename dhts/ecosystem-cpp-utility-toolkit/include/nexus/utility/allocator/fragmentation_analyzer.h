#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>

namespace nexus::utility::allocator {

/**
 * @brief Analyze heap fragmentation from a sequence of memory blocks.
 *
 * Records blocks (each marked free or used) and computes fragmentation metrics:
 * the percentage of free memory, the number of free blocks, and the size of the
 * largest contiguous free block. External fragmentation is high when there is a
 * lot of free memory split across many small blocks.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Allocator
 * @version 1.0.0
 */
class FragmentationAnalyzer {
public:
    struct Block {
        std::size_t size = 0;
        bool free = false;
    };

    /// @brief Get singleton instance
    static FragmentationAnalyzer& instance() {
        static FragmentationAnalyzer inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        blocks_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        blocks_.clear();
    }

    /// @brief Check if the utility is currently enabled
    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Enable the utility
    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    /// @brief Disable the utility
    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    // ── Domain methods ──────────────────────────────────────────────────────

    /// @brief Record a memory block of the given size and free/used state.
    void recordBlock(std::size_t size, bool free) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        blocks_.push_back({size, free});
    }

    /// @brief Percentage of total recorded memory that is free (0.0 - 100.0).
    double getFragmentationPercent() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t total = 0;
        std::size_t free_bytes = 0;
        for (const auto& b : blocks_) {
            total += b.size;
            if (b.free) free_bytes += b.size;
        }
        if (total == 0) return 0.0;
        return (static_cast<double>(free_bytes) / static_cast<double>(total)) * 100.0;
    }

    /// @brief Number of free blocks.
    std::size_t getFreeBlockCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        for (const auto& b : blocks_) {
            if (b.free) ++count;
        }
        return count;
    }

    /// @brief Size of the largest single free block.
    std::size_t getLargestFreeBlock() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t largest = 0;
        for (const auto& b : blocks_) {
            if (b.free && b.size > largest) largest = b.size;
        }
        return largest;
    }

    /// @brief Total free bytes across all free blocks.
    std::size_t getTotalFreeBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t free_bytes = 0;
        for (const auto& b : blocks_) {
            if (b.free) free_bytes += b.size;
        }
        return free_bytes;
    }

    /// @brief Total number of recorded blocks.
    std::size_t getBlockCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return blocks_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t free_count = 0;
        for (const auto& b : blocks_) {
            if (b.free) ++free_count;
        }
        return "FragmentationAnalyzer[blocks=" + std::to_string(blocks_.size()) +
               ", free_blocks=" + std::to_string(free_count) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        blocks_.clear();
    }

private:
    FragmentationAnalyzer() = default;
    ~FragmentationAnalyzer() = default;

    FragmentationAnalyzer(const FragmentationAnalyzer&) = delete;
    FragmentationAnalyzer& operator=(const FragmentationAnalyzer&) = delete;
    FragmentationAnalyzer(FragmentationAnalyzer&&) = delete;
    FragmentationAnalyzer& operator=(FragmentationAnalyzer&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Block> blocks_;
};

} // namespace nexus::utility::allocator
