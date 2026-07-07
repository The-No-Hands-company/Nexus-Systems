#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>
#include <memory>
#include <filesystem>
#include <vector>

namespace nexus::utility::logging {

// Forward declaration
enum class LogLevel;

struct LogRecord {
    std::string message;
    LogLevel level;
    // Timestamp and other metadata could be pre-formatted or passed raw
    // For simplicity in sink interface, we accept the pre-formatted string for now,
    // or the raw components if we want sinks to format differently.
    // Let's pass the pre-formatted message AND raw components for maximum flexibility.
    
    // Simplification for this phase: Sinks receive the fully formatted string
    // but we can add raw interfaces later if needed for e.g. database logging.
    std::string formatted_message;
};

/// @brief Abstract base class for log destinations
class LogSink {
public:
    virtual ~LogSink() = default;
    
    /// Write a log record to the sink
    virtual void write(const LogRecord& record) = 0;
    
    /// Flush any buffered content
    virtual void flush() = 0;
};

/// @brief Sink that writes to stdout/stderr
class ConsoleSink : public LogSink {
public:
    void write(const LogRecord& record) override {
        // We could switch based on level (cerr vs cout)
        // But locking stdout/err is good practice to prevent tearing
        // std::cout/err are thread-safe character-wise but not line-wise usually without manual sync
        // Using a mutex here or relying on Logger's serialization.
        // Let's assume the Logger serializes calls to sinks or we lock here.
        // We will lock here for safety.
        static std::mutex console_mutex;
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << record.formatted_message << std::endl;
    }

    void flush() override {
        std::cout.flush();
    }
};

/// @brief Sink that writes to a file
class FileSink : public LogSink {
public:
    explicit FileSink(const std::string& filename) {
        file_.open(filename, std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "Failed to open log file: " << filename << "\n";
        }
    }

    void write(const LogRecord& record) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_ << record.formatted_message << std::endl;
        }
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
        }
    }

private:
    std::ofstream file_;
    std::mutex mutex_;
};

/// @brief Sink that rotates files based on size
class RotatingFileSink : public LogSink {
public:
    RotatingFileSink(const std::string& base_filename, size_t max_size_bytes, int max_files)
        : base_filename_(base_filename)
        , max_size_(max_size_bytes)
        , max_files_(max_files)
    {
        openLogFile();
    }

    void write(const LogRecord& record) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!file_.is_open()) return;

        file_ << record.formatted_message << std::endl;
        current_size_ += record.formatted_message.size() + 1; // +1 for newline

        if (current_size_ >= max_size_) {
            rotate();
        }
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) file_.flush();
    }

private:
    void openLogFile() {
        file_.open(base_filename_, std::ios::app);
        if (file_.is_open()) {
            current_size_ = std::filesystem::file_size(base_filename_);
        } else {
            current_size_ = 0;
        }
    }

    void rotate() {
        file_.close();

        // Rotate existing files: log.N -> log.N+1
        namespace fs = std::filesystem;
        
        // Remove oldest
        std::string oldest = base_filename_ + "." + std::to_string(max_files_);
        if (fs::exists(oldest)) fs::remove(oldest);

        // Shift others
        for (int i = max_files_ - 1; i >= 1; --i) {
            std::string src = base_filename_ + "." + std::to_string(i);
            std::string dst = base_filename_ + "." + std::to_string(i + 1);
            if (fs::exists(src)) fs::rename(src, dst);
        }

        // Rename current
        std::string backup = base_filename_ + ".1";
        if (fs::exists(base_filename_)) fs::rename(base_filename_, backup);

        openLogFile();
    }

    std::string base_filename_;
    size_t max_size_;
    int max_files_;
    size_t current_size_ = 0;
    std::ofstream file_;
    std::mutex mutex_;
};

} // namespace nexus::utility::logging
