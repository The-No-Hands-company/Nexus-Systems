#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include <ostream>

namespace nexus::utility::config {

/// @brief Configuration validator
class ConfigValidator {
public:
    enum class ValueType {
        String,
        Integer,
        Float,
        Boolean,
        Array,
        Object
    };

    struct Rule {
        ValueType expected_type;
        bool required;
        std::any default_value;
        std::string description;
    };

    void addRule(const std::string& key, const Rule& rule);
    bool validate(const std::unordered_map<std::string, std::any>& config,
                 std::vector<std::string>& errors_out) const;

    void printSchema(std::ostream& os) const;

private:
    std::unordered_map<std::string, Rule> rules_;
};

/// @brief Environment variable accessor
class EnvironmentReader {
public:
    /// Get environment variable with default
    static std::string get(const std::string& var_name,
                          const std::string& default_value = "");

    /// Get as specific type
    static int getInt(const std::string& var_name, int default_value = 0);
    static bool getBool(const std::string& var_name, bool default_value = false);
    static double getDouble(const std::string& var_name, double default_value = 0.0);

    /// Check if variable exists
    static bool exists(const std::string& var_name);

    /// Set environment variable
    static bool set(const std::string& var_name, const std::string& value);

    /// List all environment variables
    static std::vector<std::pair<std::string, std::string>> listAll();
};

/// @brief Runtime configuration manager
class RuntimeConfig {
public:
    void set(const std::string& key, const std::any& value);

    template<typename T>
    T get(const std::string& key, const T& default_value = T()) const;

    bool has(const std::string& key) const;
    void remove(const std::string& key);

    void loadFromFile(const std::string& filename);
    void saveToFile(const std::string& filename) const;

    void printAll(std::ostream& os) const;

private:
    std::unordered_map<std::string, std::any> config_;
};

// Template implementation

template<typename T>
T RuntimeConfig::get(const std::string& key, const T& default_value) const {
    auto it = config_.find(key);
    if (it == config_.end()) {
        return default_value;
    }

    try {
        return std::any_cast<T>(it->second);
    } catch (const std::bad_any_cast&) {
        return default_value;
    }
}

} // namespace nexus::utility::config
