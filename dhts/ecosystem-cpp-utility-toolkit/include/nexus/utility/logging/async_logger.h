#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <nexus/utility/logging/log_sinks.h>
#include <memory>

namespace nexus::utility::logging {

/**
 * @brief Non-blocking asynchronous logging sink decorator.
 * Wraps another sink (or manager) and pushes logs to a background thread.
 */
class AsyncLogger : public LogSink {
public:
    static AsyncLogger& instance(); // Singleton for ease of use but also instantiable

    AsyncLogger();
    ~AsyncLogger() override;

    // LogSink interface
    void write(const LogRecord& record) override;
    void flush() override;

    /// Set the target sink(s) to write to asynchronously
    void setTargetSink(std::shared_ptr<LogSink> sink);

    /// Start the background thread
    void start();
    
    /// Stop the background thread
    void stop();

private:
    void workerLoop();

    std::shared_ptr<LogSink> target_sink_;
    
    // Double buffering could be used, but simple queue for now
    std::queue<LogRecord> queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> exit_requested_{false};
};

} // namespace nexus::utility::logging
