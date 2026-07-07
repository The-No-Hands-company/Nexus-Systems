#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::specialized {

/**
 * @brief Validate blockchain block hashes against a difficulty target.
 */
class BlockchainValidator {
public:
    struct Block {
        std::uint64_t index = 0;
        std::string previousHash;
        std::string hash;
        std::uint64_t nonce = 0;
    };

    static BlockchainValidator& instance() {
        static BlockchainValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setDifficulty(std::size_t leadingZeros) {
        std::lock_guard<std::mutex> lk(mutex_);
        difficulty_ = leadingZeros;
    }

    /// A hash meets difficulty if it begins with `difficulty_` '0' characters.
    bool meetsDifficulty(const std::string& hash) const {
        std::size_t diff;
        { std::lock_guard<std::mutex> lk(mutex_); diff = difficulty_; }
        if (hash.size() < diff) return false;
        for (std::size_t i = 0; i < diff; ++i)
            if (hash[i] != '0') return false;
        return true;
    }

    /// Append a block, validating hash difficulty and chain linkage.
    bool addBlock(const Block& block) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (block.hash.size() < difficulty_) { ++rejected_; return false; }
        for (std::size_t i = 0; i < difficulty_; ++i)
            if (block.hash[i] != '0') { ++rejected_; return false; }
        if (!chain_.empty() && chain_.back().hash != block.previousHash) {
            ++rejected_;
            return false;
        }
        chain_.push_back(block);
        return true;
    }

    /// Validate that the full chain links correctly and all hashes meet difficulty.
    bool validateChain() const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (std::size_t i = 0; i < chain_.size(); ++i) {
            const auto& b = chain_[i];
            if (b.hash.size() < difficulty_) return false;
            for (std::size_t j = 0; j < difficulty_; ++j)
                if (b.hash[j] != '0') return false;
            if (i > 0 && chain_[i - 1].hash != b.previousHash) return false;
        }
        return true;
    }

    std::size_t height() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return chain_.size();
    }
    std::size_t rejected() const { return rejected_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        chain_.clear();
        rejected_ = 0;
    }

private:
    BlockchainValidator() = default;
    ~BlockchainValidator() = default;
    BlockchainValidator(const BlockchainValidator&) = delete;
    BlockchainValidator& operator=(const BlockchainValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t difficulty_ = 4;
    std::vector<Block> chain_;
    std::atomic<std::size_t> rejected_{0};
};

} // namespace nexus::utility::specialized
