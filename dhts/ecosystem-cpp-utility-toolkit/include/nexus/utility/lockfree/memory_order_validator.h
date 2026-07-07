#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace nexus::utility::lockfree {

/**
 * @brief Validate memory ordering used in atomic operations.
 */
class MemoryOrderValidator {
public:
    enum class Operation { Load, Store, CompareExchange, FetchAdd, Fence };

    struct OperationRecord {
        Operation op;
        std::memory_order order;
        std::string variable;
    };

    static MemoryOrderValidator& instance() {
        static MemoryOrderValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordOperation(Operation op, std::memory_order order, const std::string& variable) {
        std::lock_guard<std::mutex> lk(mutex_);
        operations_.push_back({op, order, variable});
        if (!isValidOrderFor(op, order)) { warnings_.push_back(describe(op, order, variable)); }
    }

    /// A store cannot use acquire/consume; a load cannot use release.
    static bool isValidOrderFor(Operation op, std::memory_order order) {
        switch (op) {
            case Operation::Load:
                return order != std::memory_order_release &&
                       order != std::memory_order_acq_rel;
            case Operation::Store:
                return order != std::memory_order_acquire &&
                       order != std::memory_order_consume &&
                       order != std::memory_order_acq_rel;
            default:
                return true;
        }
    }

    std::vector<std::string> warnings() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return warnings_;
    }
    std::size_t operationCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return operations_.size();
    }
    bool hasWarnings() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return !warnings_.empty();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        operations_.clear();
        warnings_.clear();
    }

private:
    MemoryOrderValidator() = default;
    ~MemoryOrderValidator() = default;
    MemoryOrderValidator(const MemoryOrderValidator&) = delete;
    MemoryOrderValidator& operator=(const MemoryOrderValidator&) = delete;

    static std::string describe(Operation op, std::memory_order, const std::string& var) {
        const char* opName = "op";
        switch (op) {
            case Operation::Load: opName = "load"; break;
            case Operation::Store: opName = "store"; break;
            case Operation::CompareExchange: opName = "cas"; break;
            case Operation::FetchAdd: opName = "fetch_add"; break;
            case Operation::Fence: opName = "fence"; break;
        }
        return std::string("invalid memory order for ") + opName + " on '" + var + "'";
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<OperationRecord> operations_;
    std::vector<std::string> warnings_;
};

} // namespace nexus::utility::lockfree
