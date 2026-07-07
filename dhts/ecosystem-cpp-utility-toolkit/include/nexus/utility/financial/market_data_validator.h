#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace nexus::utility::financial {

struct PriceRange {
    double min;
    double max;
};

class MarketDataValidator {
public:
    static MarketDataValidator& instance() {
        static MarketDataValidator inst;
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
        if (!enabled_) return "MarketDataValidator disabled";
        return "MarketDataValidator active, price_ranges=" + std::to_string(price_ranges_.size())
             + ", volume_ranges=" + std::to_string(volume_ranges_.size());
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        price_ranges_.clear();
        volume_ranges_.clear();
    }

    void setPriceRange(const std::string& symbol, double min, double max) {
        std::lock_guard<std::mutex> lock(mutex_);
        price_ranges_[symbol] = PriceRange{min, max};
    }

    bool validatePrice(const std::string& symbol, double price) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return true;
        auto it = price_ranges_.find(symbol);
        if (it == price_ranges_.end()) return true;
        return price >= it->second.min && price <= it->second.max;
    }

    void setVolumeRange(const std::string& symbol, double min, double max) {
        std::lock_guard<std::mutex> lock(mutex_);
        volume_ranges_[symbol] = PriceRange{min, max};
    }

    bool validateVolume(const std::string& symbol, double volume) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return true;
        auto it = volume_ranges_.find(symbol);
        if (it == volume_ranges_.end()) return true;
        return volume >= it->second.min && volume <= it->second.max;
    }

private:
    MarketDataValidator() = default;
    ~MarketDataValidator() = default;

    MarketDataValidator(const MarketDataValidator&) = delete;
    MarketDataValidator& operator=(const MarketDataValidator&) = delete;
    MarketDataValidator(MarketDataValidator&&) = delete;
    MarketDataValidator& operator=(MarketDataValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::unordered_map<std::string, PriceRange> price_ranges_;
    std::unordered_map<std::string, PriceRange> volume_ranges_;
};

} // namespace nexus::utility::financial
