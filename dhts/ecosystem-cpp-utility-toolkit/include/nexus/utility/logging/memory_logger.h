#pragma once

#include <array>
#include <atomic>
#include <string>
#include <string_view>
#include <cstring>

namespace nexus::utility::logging {

/**
 * @brief Zero-allocation in-memory circular log buffer.
 */
template<size_t BufferSize = 65536>
class MemoryLogger {
public:
    /**
     * @brief Logs a message (zero allocation).
     */
    static void log(std::string_view message) {
        size_t msg_len = message.length();
        if (msg_len == 0) return;
        
        size_t write_pos = write_pos_.fetch_add(msg_len + 1, std::memory_order_acq_rel);
        
        // Write message to circular buffer
        for (size_t i = 0; i < msg_len; ++i) {
            buffer_[(write_pos + i) % BufferSize] = message[i];
        }
        buffer_[(write_pos + msg_len) % BufferSize] = '\n';
    }

    /**
     * @brief Retrieves recent logs.
     */
    static std::string retrieve(size_t max_bytes = BufferSize) {
        size_t current_pos = write_pos_.load(std::memory_order_acquire);
        size_t start_pos = (current_pos > max_bytes) ? (current_pos - max_bytes) : 0;
        size_t length = current_pos - start_pos;
        
        if (length > BufferSize) length = BufferSize;
        
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += buffer_[(start_pos + i) % BufferSize];
        }
        
        return result;
    }

    /**
     * @brief Clears the buffer.
     */
    static void clear() {
        write_pos_.store(0, std::memory_order_release);
        buffer_.fill(0);
    }

    /**
     * @brief Gets buffer utilization.
     */
    static float getUtilization() {
        size_t pos = write_pos_.load(std::memory_order_acquire);
        return static_cast<float>(pos % BufferSize) / BufferSize;
    }

    /**
     * @brief Gets total bytes written.
     */
    static size_t getTotalBytesWritten() {
        return write_pos_.load(std::memory_order_acquire);
    }

private:
    static inline std::atomic<size_t> write_pos_{0};
    static inline std::array<char, BufferSize> buffer_{};
};

} // namespace nexus::utility::logging
