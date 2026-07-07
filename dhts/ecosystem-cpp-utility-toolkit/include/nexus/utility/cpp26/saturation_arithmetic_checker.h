#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <limits>

namespace nexus::utility::cpp26 {

class SaturationArithmeticChecker {
public:
    struct SaturationEvent {
        std::string operation;
        int64_t value;
        int64_t limit;
        bool saturated;
    };

    template<typename T>
    static bool wouldSaturateAdd(T a, T b) {
        if (b > 0 && a > std::numeric_limits<T>::max() - b) return true;
        if (b < 0 && a < std::numeric_limits<T>::min() - b) return true;
        return false;
    }

    template<typename T>
    static bool wouldSaturateSub(T a, T b) {
        if (b < 0 && a > std::numeric_limits<T>::max() + b) return true;
        if (b > 0 && a < std::numeric_limits<T>::min() + b) return true;
        return false;
    }

    template<typename T>
    static bool wouldSaturateMul(T a, T b) {
        if (a == 0 || b == 0) return false;
        if (a > 0 && b > 0 && a > std::numeric_limits<T>::max() / b) return true;
        if (a > 0 && b < 0 && b < std::numeric_limits<T>::min() / a) return true;
        if (a < 0 && b > 0 && a < std::numeric_limits<T>::min() / b) return true;
        if (a < 0 && b < 0 && a < std::numeric_limits<T>::max() / b) return true;
        return false;
    }

    void recordSaturation(const std::string& op, int64_t val, int64_t limit, bool sat) {
        events_.push_back({op, val, limit, sat});
    }

    std::vector<SaturationEvent> getEvents() const { return events_; }

    size_t saturationCount() const {
        size_t c = 0;
        for (const auto& e : events_) if (e.saturated) c++;
        return c;
    }

    void clear() { events_.clear(); }

private:
    std::vector<SaturationEvent> events_;
};

} // namespace nexus::utility::cpp26
