#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::interop {

/**
 * @brief Validate WebAssembly interface types (core WASM value types).
 */
class WasmInterfaceValidator {
public:
    enum class ValType { I32, I64, F32, F64, V128, FuncRef, ExternRef };

    struct FuncSignature {
        std::string name;
        std::vector<ValType> params;
        std::vector<ValType> results;

        bool matches(const FuncSignature& o) const {
            return params == o.params && results == o.results;
        }
    };

    static WasmInterfaceValidator& instance() {
        static WasmInterfaceValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    static std::size_t valTypeSize(ValType t) {
        switch (t) {
            case ValType::I32: case ValType::F32: return 4;
            case ValType::I64: case ValType::F64: return 8;
            case ValType::V128: return 16;
            case ValType::FuncRef: case ValType::ExternRef: return sizeof(void*);
        }
        return 0;
    }

    void registerImport(const std::string& name, const FuncSignature& sig) {
        std::lock_guard<std::mutex> lk(mutex_);
        expected_[name] = sig;
    }

    /// Validate that a provided signature matches the registered import.
    bool validate(const std::string& name, const FuncSignature& provided) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++validations_;
        auto it = expected_.find(name);
        if (it == expected_.end()) { ++mismatches_; return false; }
        bool ok = it->second.matches(provided);
        if (!ok) ++mismatches_;
        return ok;
    }

    /// Reference types require a host with reference-type support.
    static bool usesReferenceTypes(const FuncSignature& sig) {
        auto has = [](const std::vector<ValType>& v) {
            for (auto t : v) if (t == ValType::FuncRef || t == ValType::ExternRef) return true;
            return false;
        };
        return has(sig.params) || has(sig.results);
    }

    std::size_t validations() const { return validations_.load(); }
    std::size_t mismatches() const { return mismatches_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        expected_.clear();
        validations_ = 0;
        mismatches_ = 0;
    }

private:
    WasmInterfaceValidator() = default;
    ~WasmInterfaceValidator() = default;
    WasmInterfaceValidator(const WasmInterfaceValidator&) = delete;
    WasmInterfaceValidator& operator=(const WasmInterfaceValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, FuncSignature> expected_;
    std::atomic<std::size_t> validations_{0};
    std::atomic<std::size_t> mismatches_{0};
};

} // namespace nexus::utility::interop
