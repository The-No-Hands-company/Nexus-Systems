#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <atomic>
#include <mutex>

namespace nexus::utility::documentation {

/**
 * @brief Generate usage example snippets from function signatures.
 */
class UsageExampleGenerator {
public:
    struct Parameter {
        std::string type;
        std::string name;
    };

    struct FunctionSpec {
        std::string namespaceName;
        std::string returnType;
        std::string name;
        std::vector<Parameter> params;
    };

    static UsageExampleGenerator& instance() {
        static UsageExampleGenerator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Produce a placeholder value for a type in a generated example.
    static std::string placeholder(const std::string& type) {
        if (type.find("string") != std::string::npos) return "\"example\"";
        if (type.find("bool") != std::string::npos) return "true";
        if (type.find("double") != std::string::npos ||
            type.find("float") != std::string::npos) return "1.0";
        if (type.find("int") != std::string::npos ||
            type.find("size_t") != std::string::npos) return "42";
        if (type.find('*') != std::string::npos) return "nullptr";
        return "{}";
    }

    std::string generate(const FunctionSpec& spec) {
        std::ostringstream os;
        std::string call;
        std::ostringstream callArgs;
        for (std::size_t i = 0; i < spec.params.size(); ++i) {
            if (i) callArgs << ", ";
            callArgs << placeholder(spec.params[i].type);
        }
        std::string qualified = spec.namespaceName.empty()
            ? spec.name : spec.namespaceName + "::" + spec.name;
        if (spec.returnType != "void" && !spec.returnType.empty())
            os << "auto result = ";
        os << qualified << "(" << callArgs.str() << ");";
        std::string example = os.str();
        std::lock_guard<std::mutex> lk(mutex_);
        examples_.push_back(example);
        return example;
    }

    std::vector<std::string> examples() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return examples_;
    }

    std::size_t count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return examples_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        examples_.clear();
    }

private:
    UsageExampleGenerator() = default;
    ~UsageExampleGenerator() = default;
    UsageExampleGenerator(const UsageExampleGenerator&) = delete;
    UsageExampleGenerator& operator=(const UsageExampleGenerator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<std::string> examples_;
};

} // namespace nexus::utility::documentation
