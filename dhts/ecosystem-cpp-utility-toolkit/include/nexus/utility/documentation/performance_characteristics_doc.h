#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::documentation {

/**
 * @brief Document performance characteristics (complexity, thread-safety) of APIs.
 */
class PerformanceCharacteristicsDoc {
public:
    enum class Complexity { O1, OLogN, ON, ONLogN, ON2, ON3, OExp, Unknown };

    struct Characteristics {
        std::string symbol;
        Complexity timeComplexity = Complexity::Unknown;
        Complexity spaceComplexity = Complexity::Unknown;
        bool threadSafe = false;
        bool allocates = false;
        bool mayThrow = false;
        std::string notes;
    };

    static PerformanceCharacteristicsDoc& instance() {
        static PerformanceCharacteristicsDoc inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static const char* complexityString(Complexity c) {
        switch (c) {
            case Complexity::O1:     return "O(1)";
            case Complexity::OLogN:  return "O(log n)";
            case Complexity::ON:     return "O(n)";
            case Complexity::ONLogN: return "O(n log n)";
            case Complexity::ON2:    return "O(n^2)";
            case Complexity::ON3:    return "O(n^3)";
            case Complexity::OExp:   return "O(2^n)";
            case Complexity::Unknown:return "unknown";
        }
        return "unknown";
    }

    void document(const Characteristics& c) {
        std::lock_guard<std::mutex> lk(mutex_);
        docs_[c.symbol] = c;
    }

    Characteristics get(const std::string& symbol) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = docs_.find(symbol);
        return it == docs_.end() ? Characteristics{symbol} : it->second;
    }

    /// Symbols with worse than the given complexity (for hotspot review).
    std::vector<Characteristics> worseThan(Complexity threshold) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Characteristics> out;
        for (const auto& [s, c] : docs_)
            if (static_cast<int>(c.timeComplexity) > static_cast<int>(threshold) &&
                c.timeComplexity != Complexity::Unknown)
                out.push_back(c);
        return out;
    }

    std::size_t count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return docs_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        docs_.clear();
    }

private:
    PerformanceCharacteristicsDoc() = default;
    ~PerformanceCharacteristicsDoc() = default;
    PerformanceCharacteristicsDoc(const PerformanceCharacteristicsDoc&) = delete;
    PerformanceCharacteristicsDoc& operator=(const PerformanceCharacteristicsDoc&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Characteristics> docs_;
};

} // namespace nexus::utility::documentation
