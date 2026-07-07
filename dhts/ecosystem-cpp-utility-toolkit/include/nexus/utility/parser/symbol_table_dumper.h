#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace nexus::utility::parser {

/// @brief Collects and dumps symbol table entries.
class SymbolTableDumper {
public:
    struct Symbol {
        std::string name;
        std::string type;
        std::string scope;
        size_t line = 0;
    };

    static SymbolTableDumper& instance() {
        static SymbolTableDumper inst;
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
        symbols_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addSymbol(const std::string& name, const std::string& type,
                   const std::string& scope, size_t line) {
        std::lock_guard<std::mutex> lock(mutex_);
        symbols_.push_back({name, type, scope, line});
    }

    std::vector<Symbol> getSymbols() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return symbols_;
    }

    std::vector<Symbol> getSymbolsByScope(const std::string& scope) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Symbol> out;
        for (const auto& s : symbols_) {
            if (s.scope == scope) out.push_back(s);
        }
        return out;
    }

    size_t getSymbolCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return symbols_.size();
    }

    /// @brief Look up the most recently added symbol with the given name.
    std::optional<Symbol> lookup(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = symbols_.rbegin(); it != symbols_.rend(); ++it) {
            if (it->name == name) return *it;
        }
        return std::nullopt;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        symbols_.clear();
    }

private:
    SymbolTableDumper() = default;
    ~SymbolTableDumper() = default;
    SymbolTableDumper(const SymbolTableDumper&) = delete;
    SymbolTableDumper& operator=(const SymbolTableDumper&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Symbol> symbols_;
};

} // namespace nexus::utility::parser
