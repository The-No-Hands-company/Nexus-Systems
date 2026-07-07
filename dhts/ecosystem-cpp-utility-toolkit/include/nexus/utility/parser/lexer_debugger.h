#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace nexus::utility::parser {

/// @brief Records tokens emitted by a lexer for inspection and debugging.
class LexerDebugger {
public:
    struct Token {
        std::string type;
        std::string value;
        size_t line = 0;
        size_t col = 0;
    };

    static LexerDebugger& instance() {
        static LexerDebugger inst;
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
        tokens_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordToken(const std::string& type, const std::string& value, size_t line, size_t col) {
        std::lock_guard<std::mutex> lock(mutex_);
        tokens_.push_back({type, value, line, col});
    }

    std::vector<Token> getTokens() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tokens_;
    }

    std::vector<Token> getTokensByType(const std::string& type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Token> out;
        for (const auto& t : tokens_) {
            if (t.type == type) out.push_back(t);
        }
        return out;
    }

    std::vector<Token> getTokensOnLine(size_t line) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Token> out;
        for (const auto& t : tokens_) {
            if (t.line == line) out.push_back(t);
        }
        return out;
    }

    size_t getTokenCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tokens_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        tokens_.clear();
    }

private:
    LexerDebugger() = default;
    ~LexerDebugger() = default;
    LexerDebugger(const LexerDebugger&) = delete;
    LexerDebugger& operator=(const LexerDebugger&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Token> tokens_;
};

} // namespace nexus::utility::parser
