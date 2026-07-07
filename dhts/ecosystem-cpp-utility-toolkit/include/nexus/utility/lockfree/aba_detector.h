#pragma once

#include <atomic>
#include <vector>
#include <string>
#include <chrono>
#include <ostream>
#include <unordered_map>

namespace nexus::utility::lockfree {

/// @brief Detect ABA problem in lock-free structures
template<typename T>
class ABADetector {
public:
    struct VersionedPointer {
        T* ptr;
        uint64_t version;

        VersionedPointer() : ptr(nullptr), version(0) {}
        VersionedPointer(T* p, uint64_t v) : ptr(p), version(v) {}
    };

    void recordAccess(T* ptr, uint64_t version);
    bool detectABA(T* ptr, uint64_t old_version, uint64_t new_version);

    void printReport(std::ostream& os) const;

private:
    struct AccessRecord {
        T* ptr;
        uint64_t version;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::vector<AccessRecord> access_history_;
    std::atomic<size_t> aba_detected_{0};
};

/// @brief Validate memory ordering
class MemoryOrderValidator {
public:
    enum class Operation {
        Load,
        Store,
        CompareExchange,
        FetchAdd
    };

    void recordOperation(Operation op,
                        std::memory_order order,
                        const std::string& variable_name);

    bool validateOrdering(std::vector<std::string>& warnings_out) const;

    void printReport(std::ostream& os) const;

private:
    struct OperationRecord {
        Operation op;
        std::memory_order order;
        std::string variable_name;
    };

    std::vector<OperationRecord> operations_;
};

/// @brief Monitor lock-free progress guarantees
class LockFreeProgressMonitor {
public:
    enum class ProgressGuarantee {
        WaitFree,      // All threads make progress
        LockFree,      // System makes progress
        ObstructionFree, // Threads make progress in isolation
        Blocking       // No progress guarantee
    };

    void recordThreadAttempt(size_t thread_id, bool success);

    ProgressGuarantee estimateGuarantee() const;

    struct ProgressStats {
        size_t total_attempts = 0;
        size_t successful_attempts = 0;
        size_t failed_attempts = 0;
        size_t max_retries = 0;
    };

    ProgressStats getStats() const;
    void printReport(std::ostream& os) const;

private:
    struct ThreadStats {
        size_t attempts = 0;
        size_t successes = 0;
        size_t consecutive_failures = 0;
        size_t max_consecutive_failures = 0;
    };

    std::unordered_map<size_t, ThreadStats> thread_stats_;
};

/// @brief Trace compare-exchange operations
class CompareExchangeTracer {
public:
    void recordCAS(const std::string& variable_name,
                  bool success,
                  const std::string& location);

    struct CASStats {
        std::string variable_name;
        size_t total_attempts = 0;
        size_t successes = 0;
        size_t failures = 0;
        float success_rate = 0.0f;
    };

    std::vector<CASStats> getStats() const;
    void printReport(std::ostream& os) const;

private:
    struct CASRecord {
        std::string variable_name;
        bool success;
        std::string location;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::vector<CASRecord> cas_history_;
};

// Template implementations

template<typename T>
void ABADetector<T>::recordAccess(T* ptr, uint64_t version) {
    access_history_.push_back({
        ptr,
        version,
        std::chrono::steady_clock::now()
    });
}

template<typename T>
bool ABADetector<T>::detectABA(T* ptr, uint64_t old_version, uint64_t new_version) {
    // ABA occurs when pointer is the same but version changed
    if (old_version != new_version) {
        // Check if pointer was accessed with different versions
        bool found_old = false;
        bool found_new = false;

        for (const auto& record : access_history_) {
            if (record.ptr == ptr) {
                if (record.version == old_version) found_old = true;
                if (record.version == new_version) found_new = true;
            }
        }

        if (found_old && found_new) {
            aba_detected_++;
            return true;
        }
    }

    return false;
}

template<typename T>
void ABADetector<T>::printReport(std::ostream& os) const {
    os << "\n=== ABA Problem Detection ===\n";
    os << "ABA problems detected: " << aba_detected_.load() << "\n";
    os << "Total accesses tracked: " << access_history_.size() << "\n";
    os << "=============================\n";
}

} // namespace nexus::utility::lockfree
