#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <ostream>
#include <unordered_map>

namespace nexus::utility::api {

/// @brief API contract validator
class ContractValidator {
public:
    struct Precondition {
        std::function<bool()> check;
        std::string description;
    };

    struct Postcondition {
        std::function<bool()> check;
        std::string description;
    };

    void addPrecondition(Precondition condition);
    void addPostcondition(Postcondition condition);

    bool validatePreconditions(std::vector<std::string>& errors_out) const;
    bool validatePostconditions(std::vector<std::string>& errors_out) const;

private:
    std::vector<Precondition> preconditions_;
    std::vector<Postcondition> postconditions_;
};

/// @brief Interface compatibility checker
class InterfaceValidator {
public:
    struct MethodSignature {
        std::string name;
        std::vector<std::string> parameter_types;
        std::string return_type;
    };

    void registerInterface(const std::string& interface_name,
                          const std::vector<MethodSignature>& methods);

    bool isCompatible(const std::string& interface_name,
                     const std::vector<MethodSignature>& implementation,
                     std::vector<std::string>& errors_out) const;

private:
    std::unordered_map<std::string, std::vector<MethodSignature>> interfaces_;
};

/// @brief Function call tracer
class FunctionCallTracer {
public:
    void enterFunction(const std::string& function_name,
                      const std::vector<std::string>& arguments = {});
    void exitFunction(const std::string& function_name,
                     const std::string& return_value = "");

    void printCallStack(std::ostream& os) const;
    size_t getStackDepth() const;

    void clear();

private:
    struct StackFrame {
        std::string function_name;
        std::vector<std::string> arguments;
        std::chrono::steady_clock::time_point enter_time;
    };

    std::vector<StackFrame> call_stack_;
};

} // namespace nexus::utility::api
