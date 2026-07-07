#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <set>

namespace nexus::utility::config {

class ConfigSchemaValidator {
public:
    static ConfigSchemaValidator& instance() {
        static ConfigSchemaValidator inst;
        return inst;
    }

    void initialize() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    void addSchema(const std::string& key, const std::string& type, bool required) {
        std::lock_guard<std::mutex> lk(mutex_);
        schema_[key] = {type, required};
    }

    std::vector<std::string> validate(const std::map<std::string, std::string>& config) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::string> errors;
        static const std::set<std::string> valid_types = {"string", "int", "bool", "float", "path"};

        for (const auto& [key, rule] : schema_) {
            if (valid_types.find(rule.type) == valid_types.end()) {
                errors.push_back("schema key '" + key + "': unknown type '" + rule.type + "'");
                continue;
            }
            auto it = config.find(key);
            if (it == config.end()) {
                if (rule.required) {
                    errors.push_back("missing required key '" + key + "'");
                }
                continue;
            }
            if (rule.type == "int") {
                try { auto v = std::stoll(it->second); (void)v; }
                catch (...) { errors.push_back("key '" + key + "': expected int, got '" + it->second + "'"); }
            } else if (rule.type == "float") {
                try { auto v = std::stod(it->second); (void)v; }
                catch (...) { errors.push_back("key '" + key + "': expected float, got '" + it->second + "'"); }
            } else if (rule.type == "bool") {
                std::string v = it->second;
                if (v != "true" && v != "false" && v != "0" && v != "1") {
                    errors.push_back("key '" + key + "': expected bool, got '" + v + "'");
                }
            }
        }
        return errors;
    }

private:
    struct SchemaRule {
        std::string type;
        bool required;
    };

    ConfigSchemaValidator() = default;
    ~ConfigSchemaValidator() = default;
    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<std::string, SchemaRule> schema_;
};

} // namespace nexus::utility::config
