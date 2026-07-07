#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::scientific {

/**
 * @brief Validate dimensional (unit) consistency of physical quantities.
 *
 * Units are represented as exponents over SI base dimensions:
 * [length, mass, time, current, temperature, amount, luminous intensity].
 */
class UnitConsistencyChecker {
public:
    using Dimension = std::array<int, 7>;

    struct Quantity {
        std::string name;
        Dimension dim{0, 0, 0, 0, 0, 0, 0};
    };

    static UnitConsistencyChecker& instance() {
        static UnitConsistencyChecker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static Dimension length()  { return {1,0,0,0,0,0,0}; }
    static Dimension mass()    { return {0,1,0,0,0,0,0}; }
    static Dimension time()    { return {0,0,1,0,0,0,0}; }
    static Dimension velocity(){ return {1,0,-1,0,0,0,0}; }
    static Dimension force()   { return {1,1,-2,0,0,0,0}; }

    static Dimension multiply(const Dimension& a, const Dimension& b) {
        Dimension r{};
        for (std::size_t i = 0; i < 7; ++i) r[i] = a[i] + b[i];
        return r;
    }
    static Dimension divide(const Dimension& a, const Dimension& b) {
        Dimension r{};
        for (std::size_t i = 0; i < 7; ++i) r[i] = a[i] - b[i];
        return r;
    }

    /// Addition/subtraction requires identical dimensions.
    static bool compatibleForAddition(const Dimension& a, const Dimension& b) {
        return a == b;
    }

    void declare(const std::string& name, const Dimension& dim) {
        std::lock_guard<std::mutex> lk(mutex_);
        quantities_[name] = {name, dim};
    }

    /// Verify an assignment target = expr matches dimensions; records mismatches.
    bool checkAssignment(const Dimension& target, const Dimension& expr) {
        bool ok = (target == expr);
        std::lock_guard<std::mutex> lk(mutex_);
        ++checks_;
        if (!ok) ++mismatches_;
        return ok;
    }

    std::size_t mismatches() const { return mismatches_.load(); }
    std::size_t checks() const { return checks_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        quantities_.clear();
        checks_ = 0;
        mismatches_ = 0;
    }

private:
    UnitConsistencyChecker() = default;
    ~UnitConsistencyChecker() = default;
    UnitConsistencyChecker(const UnitConsistencyChecker&) = delete;
    UnitConsistencyChecker& operator=(const UnitConsistencyChecker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Quantity> quantities_;
    std::atomic<std::size_t> checks_{0};
    std::atomic<std::size_t> mismatches_{0};
};

} // namespace nexus::utility::scientific
