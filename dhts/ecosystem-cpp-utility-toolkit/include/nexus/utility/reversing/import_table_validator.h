#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::reversing {

/**
 * @brief Validate a PE/ELF import table against expected imports.
 */
class ImportTableValidator {
public:
    struct Import {
        std::string module;      // e.g. "kernel32.dll" or "libc.so.6"
        std::string symbol;      // e.g. "CreateFileW"
        std::uint64_t address = 0;
        bool resolved = false;
    };

    static ImportTableValidator& instance() {
        static ImportTableValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Register an import that is expected to be present.
    void expectImport(const std::string& module, const std::string& symbol) {
        std::lock_guard<std::mutex> lk(mutex_);
        expected_.push_back(key(module, symbol));
    }

    void recordImport(const std::string& module, const std::string& symbol,
                      std::uint64_t address, bool resolved = true) {
        std::lock_guard<std::mutex> lk(mutex_);
        imports_[key(module, symbol)] = {module, symbol, address, resolved};
    }

    bool hasImport(const std::string& module, const std::string& symbol) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return imports_.count(key(module, symbol)) > 0;
    }

    /// Imports that were expected but not found in the table.
    std::vector<std::string> missingImports() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::string> out;
        for (const auto& k : expected_)
            if (!imports_.count(k)) out.push_back(k);
        return out;
    }

    /// Imports that failed to resolve to a valid address.
    std::vector<Import> unresolvedImports() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Import> out;
        for (const auto& [k, imp] : imports_)
            if (!imp.resolved || imp.address == 0) out.push_back(imp);
        return out;
    }

    bool validate() const {
        return missingImports().empty() && unresolvedImports().empty();
    }

    std::size_t importCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return imports_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        imports_.clear();
        expected_.clear();
    }

private:
    ImportTableValidator() = default;
    ~ImportTableValidator() = default;
    ImportTableValidator(const ImportTableValidator&) = delete;
    ImportTableValidator& operator=(const ImportTableValidator&) = delete;

    static std::string key(const std::string& module, const std::string& symbol) {
        return module + "!" + symbol;
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Import> imports_;
    std::vector<std::string> expected_;
};

} // namespace nexus::utility::reversing
