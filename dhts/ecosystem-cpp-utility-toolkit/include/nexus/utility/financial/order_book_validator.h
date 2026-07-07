#pragma once

#include <mutex>
#include <string>

namespace nexus::utility::financial {

class OrderBookValidator {
public:
    static OrderBookValidator& instance() {
        static OrderBookValidator inst;
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
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return "OrderBookValidator disabled";
        return "OrderBookValidator active, violations=" + std::to_string(violation_count_);
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        violation_count_ = 0;
    }

    bool validateBidAsk(double bid, double ask) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return true;
        if (bid >= ask) {
            ++violation_count_;
            return false;
        }
        return true;
    }

    bool validateQuantity(double qty) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return true;
        if (qty <= 0.0) {
            ++violation_count_;
            return false;
        }
        return true;
    }

    bool validateOrder(const std::string& order_id, const std::string& side, double price, double qty) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return true;
        bool valid = true;
        if (price <= 0.0) {
            ++violation_count_;
            valid = false;
        }
        if (qty <= 0.0) {
            ++violation_count_;
            valid = false;
        }
        if (side != "BUY" && side != "SELL") {
            ++violation_count_;
            valid = false;
        }
        return valid;
    }

    uint64_t getViolationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violation_count_;
    }

private:
    OrderBookValidator() = default;
    ~OrderBookValidator() = default;

    OrderBookValidator(const OrderBookValidator&) = delete;
    OrderBookValidator& operator=(const OrderBookValidator&) = delete;
    OrderBookValidator(OrderBookValidator&&) = delete;
    OrderBookValidator& operator=(OrderBookValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    uint64_t violation_count_ = 0;
};

} // namespace nexus::utility::financial
