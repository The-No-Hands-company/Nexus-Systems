#pragma once

#include <exception>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <sstream>

namespace nexus::utility::crash {

/**
 * @brief Traces exception propagation and history.
 */
class ExceptionTracer {
public:
    struct ExceptionEvent {
        std::string type_name;
        std::string message;
        std::string throw_site;
        std::chrono::steady_clock::time_point timestamp;
        bool caught = false;
    };

    /**
     * @brief Records an exception throw.
     */
    static void recordThrow(const std::exception& ex, const std::string& site = "") {
        std::lock_guard lock(mutex_);
        
        ExceptionEvent event;
        event.type_name = typeid(ex).name();
        event.message = ex.what();
        event.throw_site = site;
        event.timestamp = std::chrono::steady_clock::now();
        
        history_.push_back(event);
    }

    /**
     * @brief Records an exception catch.
     */
    static void recordCatch(const std::exception& ex) {
        std::lock_guard lock(mutex_);
        
        // Mark last matching exception as caught
        for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
            if (it->type_name == typeid(ex).name() && !it->caught) {
                it->caught = true;
                break;
            }
        }
    }

    /**
     * @brief Gets exception history.
     */
    static std::vector<ExceptionEvent> getHistory() {
        std::lock_guard lock(mutex_);
        return history_;
    }

    /**
     * @brief Gets uncaught exceptions.
     */
    static std::vector<ExceptionEvent> getUncaught() {
        std::lock_guard lock(mutex_);
        std::vector<ExceptionEvent> uncaught;
        
        for (const auto& event : history_) {
            if (!event.caught) {
                uncaught.push_back(event);
            }
        }
        
        return uncaught;
    }

    /**
     * @brief Generates exception trace report.
     */
    static std::string generateReport() {
        std::lock_guard lock(mutex_);
        std::ostringstream report;
        
        report << "=== Exception Trace Report ===\n";
        report << "Total Exceptions: " << history_.size() << "\n";
        
        size_t uncaught_count = 0;
        for (const auto& event : history_) {
            if (!event.caught) uncaught_count++;
        }
        report << "Uncaught: " << uncaught_count << "\n\n";
        
        for (const auto& event : history_) {
            report << "Type: " << event.type_name << "\n";
            report << "Message: " << event.message << "\n";
            report << "Status: " << (event.caught ? "Caught" : "Uncaught") << "\n";
            if (!event.throw_site.empty()) {
                report << "Site: " << event.throw_site << "\n";
            }
            report << "---\n";
        }
        
        return report.str();
    }

    /**
     * @brief Resets history.
     */
    static void reset() {
        std::lock_guard lock(mutex_);
        history_.clear();
    }

private:
    static inline std::mutex mutex_;
    static inline std::vector<ExceptionEvent> history_;
};

// Convenience macros
#define NEXUS_TRACE_THROW(ex) \
    nexus::utility::crash::ExceptionTracer::recordThrow(ex, __FILE__ ":" + std::to_string(__LINE__))

#define NEXUS_TRACE_CATCH(ex) \
    nexus::utility::crash::ExceptionTracer::recordCatch(ex)

} // namespace nexus::utility::crash