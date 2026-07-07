#pragma once

#include <string>
#include <vector>
#include <random>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::binary {

/**
 * @brief Generate mutated test inputs for binary-format parsers (fuzzing).
 */
class BinaryFormatFuzzer {
public:
    enum class Mutation { BitFlip, ByteFlip, ByteInsert, ByteDelete, Truncate, Duplicate };

    static BinaryFormatFuzzer& instance() {
        static BinaryFormatFuzzer inst;
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

    /// Produce a mutated copy of `input`.
    std::vector<std::uint8_t> mutate(const std::vector<std::uint8_t>& input) {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::uint8_t> out = input;
        auto pick = [&](std::size_t n) { return n ? rng_() % n : 0; };
        Mutation m = static_cast<Mutation>(rng_() % 6);
        switch (m) {
            case Mutation::BitFlip:
                if (!out.empty()) out[pick(out.size())] ^= (1u << (rng_() % 8));
                break;
            case Mutation::ByteFlip:
                if (!out.empty()) out[pick(out.size())] = static_cast<std::uint8_t>(rng_() & 0xFF);
                break;
            case Mutation::ByteInsert:
                out.insert(out.begin() + (out.empty() ? 0 : pick(out.size())),
                           static_cast<std::uint8_t>(rng_() & 0xFF));
                break;
            case Mutation::ByteDelete:
                if (!out.empty()) out.erase(out.begin() + pick(out.size()));
                break;
            case Mutation::Truncate:
                if (!out.empty()) out.resize(pick(out.size()));
                break;
            case Mutation::Duplicate:
                if (!out.empty()) {
                    std::size_t i = pick(out.size());
                    out.insert(out.begin() + i, out[i]);
                }
                break;
        }
        ++generated_;
        return out;
    }

    /// Produce `count` mutated variants of the seed input.
    std::vector<std::vector<std::uint8_t>> generateCorpus(
        const std::vector<std::uint8_t>& input, std::size_t count) {
        std::vector<std::vector<std::uint8_t>> corpus;
        corpus.reserve(count);
        for (std::size_t i = 0; i < count; ++i) corpus.push_back(mutate(input));
        return corpus;
    }

    std::size_t generated() const { return generated_.load(); }

    void reset() { generated_ = 0; }

private:
    BinaryFormatFuzzer() : rng_(0xC0FFEE) {}
    ~BinaryFormatFuzzer() = default;
    BinaryFormatFuzzer(const BinaryFormatFuzzer&) = delete;
    BinaryFormatFuzzer& operator=(const BinaryFormatFuzzer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::mt19937_64 rng_;
    std::atomic<std::size_t> generated_{0};
};

} // namespace nexus::utility::binary
