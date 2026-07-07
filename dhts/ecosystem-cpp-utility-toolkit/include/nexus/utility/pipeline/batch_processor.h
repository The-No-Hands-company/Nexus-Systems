#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::pipeline {

/// @brief Buffers items and dispatches them to a processor in fixed-size batches.
class BatchProcessor {
public:
    using Processor = std::function<void(std::vector<std::string>&)>;

    static BatchProcessor& instance() {
        static BatchProcessor inst;
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
        buffer_.clear();
        item_count_ = 0;
        batch_count_ = 0;
        processor_ = nullptr;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addItem(const std::string& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.push_back(item);
        ++item_count_;
    }

    /// @brief Dispatch full batches of batch_size to the processor.
    /// @return number of batches processed.
    size_t processBatch(size_t batch_size, Processor processor) {
        if (batch_size == 0) return 0;
        size_t processed = 0;
        while (true) {
            std::vector<std::string> batch;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                processor_ = processor;
                if (buffer_.size() < batch_size) break;
                batch.assign(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(batch_size));
                buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(batch_size));
            }
            if (processor) processor(batch);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++batch_count_;
            }
            ++processed;
        }
        return processed;
    }

    /// @brief Process any remaining buffered items using the last processor.
    /// @return true if a partial batch was flushed.
    bool flush() {
        std::vector<std::string> batch;
        Processor processor;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (buffer_.empty() || !processor_) return false;
            batch.swap(buffer_);
            processor = processor_;
        }
        processor(batch);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++batch_count_;
        }
        return true;
    }

    size_t getItemCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return item_count_;
    }

    size_t getBatchCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return batch_count_;
    }

    size_t getPendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.clear();
        item_count_ = 0;
        batch_count_ = 0;
        processor_ = nullptr;
    }

private:
    BatchProcessor() = default;
    ~BatchProcessor() = default;
    BatchProcessor(const BatchProcessor&) = delete;
    BatchProcessor& operator=(const BatchProcessor&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<std::string> buffer_;
    Processor processor_;
    size_t item_count_ = 0;
    size_t batch_count_ = 0;
};

} // namespace nexus::utility::pipeline
