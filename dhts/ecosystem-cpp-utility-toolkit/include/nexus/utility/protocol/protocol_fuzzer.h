#pragma once

#include <string>
#include <vector>
#include <random>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::protocol {

/**
 * @brief Generate fuzzed protocol messages from a template/grammar.
 */
class ProtocolFuzzer {
public:
    struct Field {
        std::string name;
        std::size_t minLen = 0;
        std::size_t maxLen = 0;
        bool numeric = false;
    };

    static ProtocolFuzzer& instance() {
        static ProtocolFuzzer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void seed(std::uint64_t s) {
        std::lock_guard<std::mutex> lk(mutex_);
        rng_.seed(s);
    }

    void addField(const std::string& name, std::size_t minLen, std::size_t maxLen,
                  bool numeric = false) {
        std::lock_guard<std::mutex> lk(mutex_);
        fields_.push_back({name, minLen, maxLen, numeric});
    }

    /// Generate a single fuzzed message based on registered fields.
    std::vector<std::uint8_t> generate() {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::uint8_t> msg;
        for (const auto& f : fields_) {
            std::size_t span = f.maxLen >= f.minLen ? f.maxLen - f.minLen : 0;
            std::size_t len = f.minLen + (span ? rng_() % (span + 1) : 0);
            for (std::size_t i = 0; i < len; ++i) {
                std::uint8_t b = f.numeric
                    ? static_cast<std::uint8_t>('0' + rng_() % 10)
                    : static_cast<std::uint8_t>(rng_() & 0xFF);
                msg.push_back(b);
            }
        }
        ++generated_;
        return msg;
    }

    /// Generate a batch of fuzzed messages.
    std::vector<std::vector<std::uint8_t>> generateBatch(std::size_t count) {
        std::vector<std::vector<std::uint8_t>> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i) out.push_back(generate());
        return out;
    }

    std::size_t generated() const { return generated_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        fields_.clear();
        generated_ = 0;
    }

private:
    ProtocolFuzzer() : rng_(0xBADC0DE) {}
    ~ProtocolFuzzer() = default;
    ProtocolFuzzer(const ProtocolFuzzer&) = delete;
    ProtocolFuzzer& operator=(const ProtocolFuzzer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Field> fields_;
    std::mt19937_64 rng_;
    std::atomic<std::size_t> generated_{0};
};

} // namespace nexus::utility::protocol
