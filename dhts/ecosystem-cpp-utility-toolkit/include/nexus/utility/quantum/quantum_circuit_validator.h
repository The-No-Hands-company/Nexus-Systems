#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace nexus::utility::quantum {

/**
 * @brief Validate quantum circuit gates and qubit references.
 */
class QuantumCircuitValidator {
public:
    struct Gate {
        std::string name;                 // "H", "X", "CNOT", "RZ", ...
        std::vector<std::size_t> qubits;  // target/control qubits
        double parameter = 0.0;           // for parameterized gates
    };

    struct ValidationResult {
        bool valid = true;
        std::vector<std::string> errors;
    };

    static QuantumCircuitValidator& instance() {
        static QuantumCircuitValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setQubitCount(std::size_t n) {
        std::lock_guard<std::mutex> lk(mutex_);
        qubitCount_ = n;
    }

    /// Expected arity for a named gate (0 = variadic/unknown).
    static std::size_t gateArity(const std::string& name) {
        if (name == "H" || name == "X" || name == "Y" || name == "Z" ||
            name == "S" || name == "T" || name == "RX" || name == "RY" ||
            name == "RZ" || name == "MEASURE") return 1;
        if (name == "CNOT" || name == "CX" || name == "CZ" || name == "SWAP") return 2;
        if (name == "TOFFOLI" || name == "CCX") return 3;
        return 0;
    }

    void addGate(const Gate& gate) {
        std::lock_guard<std::mutex> lk(mutex_);
        gates_.push_back(gate);
    }

    ValidationResult validate() {
        std::lock_guard<std::mutex> lk(mutex_);
        ValidationResult r;
        for (std::size_t i = 0; i < gates_.size(); ++i) {
            const auto& g = gates_[i];
            std::size_t arity = gateArity(g.name);
            if (arity != 0 && g.qubits.size() != arity) {
                r.valid = false;
                r.errors.push_back("gate " + g.name + " at index " + std::to_string(i) +
                                   " has wrong qubit count");
            }
            for (std::size_t q : g.qubits) {
                if (qubitCount_ != 0 && q >= qubitCount_) {
                    r.valid = false;
                    r.errors.push_back("gate " + g.name + " references out-of-range qubit " +
                                       std::to_string(q));
                }
            }
            std::vector<std::size_t> seen;
            for (std::size_t q : g.qubits) {
                for (std::size_t s : seen)
                    if (s == q) { r.valid = false;
                        r.errors.push_back("gate " + g.name + " has duplicate qubit"); }
                seen.push_back(q);
            }
        }
        if (!r.valid) ++invalidValidations_;
        return r;
    }

    std::size_t depth() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return gates_.size();
    }
    std::size_t invalidValidations() const { return invalidValidations_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        gates_.clear();
        invalidValidations_ = 0;
    }

private:
    QuantumCircuitValidator() = default;
    ~QuantumCircuitValidator() = default;
    QuantumCircuitValidator(const QuantumCircuitValidator&) = delete;
    QuantumCircuitValidator& operator=(const QuantumCircuitValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::size_t qubitCount_ = 0;
    std::vector<Gate> gates_;
    std::atomic<std::size_t> invalidValidations_{0};
};

} // namespace nexus::utility::quantum
