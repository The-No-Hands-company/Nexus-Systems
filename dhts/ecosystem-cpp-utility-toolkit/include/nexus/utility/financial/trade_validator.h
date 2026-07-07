#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus::utility::financial {

class TradeValidator {
public:
    static TradeValidator& instance() {
        static TradeValidator inst;
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
        if (!enabled_) return "TradeValidator disabled";
        return "TradeValidator active, violations=" + std::to_string(violations_.size())
             + ", max_qty_rules=" + std::to_string(max_quantities_.size())
             + ", min_price_rules=" + std::to_string(min_prices_.size());
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        violations_.clear();
        max_quantities_.clear();
        min_prices_.clear();
    }

    bool validateTrade(const std::string& symbol, double price, double quantity, const std::string& side) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return true;
        bool valid = true;

        if (side != "BUY" && side != "SELL") {
            violations_.push_back("invalid_side:" + side);
            valid = false;
        }
        if (price <= 0.0) {
            violations_.push_back("invalid_price:" + symbol);
            valid = false;
        }
        if (quantity <= 0.0) {
            violations_.push_back("invalid_quantity:" + symbol);
            valid = false;
        }

        auto qty_it = max_quantities_.find(symbol);
        if (qty_it != max_quantities_.end() && quantity > qty_it->second) {
            violations_.push_back("max_quantity_exceeded:" + symbol);
            valid = false;
        }

        auto price_it = min_prices_.find(symbol);
        if (price_it != min_prices_.end() && price < price_it->second) {
            violations_.push_back("min_price_violation:" + symbol);
            valid = false;
        }

        return valid;
    }

    void setMaxQuantity(const std::string& symbol, double max_qty) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_quantities_[symbol] = max_qty;
    }

    void setMinPrice(const std::string& symbol, double min_price) {
        std::lock_guard<std::mutex> lock(mutex_);
        min_prices_[symbol] = min_price;
    }

    std::vector<std::string> getViolations() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_;
    }

private:
    TradeValidator() = default;
    ~TradeValidator() = default;

    TradeValidator(const TradeValidator&) = delete;
    TradeValidator& operator=(const TradeValidator&) = delete;
    TradeValidator(TradeValidator&&) = delete;
    TradeValidator& operator=(TradeValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<std::string> violations_;
    std::unordered_map<std::string, double> max_quantities_;
    std::unordered_map<std::string, double> min_prices_;
};

} // namespace nexus::utility::financial
