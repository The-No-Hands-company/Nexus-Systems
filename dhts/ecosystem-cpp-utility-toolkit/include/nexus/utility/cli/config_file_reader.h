#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <optional>
#include <stdexcept>
#include <algorithm>

namespace nexus::utility::cli {

/**
 * @brief Generic key=value configuration file reader.
 */
class ConfigFileReader {
public:
    /**
     * @brief Loads configuration from a file.
     */
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            parseLine(line);
        }

        return true;
    }

    /**
     * @brief Gets a value as a specific type.
     */
    template<typename T>
    T get(const std::string& key, T default_value = T{}) const {
        auto it = values_.find(key);
        if (it == values_.end()) {
            return default_value;
        }

        return convertValue<T>(it->second);
    }

    /**
     * @brief Checks if a key exists.
     */
    bool has(const std::string& key) const {
        return values_.find(key) != values_.end();
    }

    /**
     * @brief Gets all keys.
     */
    std::vector<std::string> keys() const {
        std::vector<std::string> result;
        for (const auto& [key, _] : values_) {
            result.push_back(key);
        }
        return result;
    }

    /**
     * @brief Sets a value programmatically.
     */
    void set(const std::string& key, const std::string& value) {
        values_[key] = value;
    }

private:
    void parseLine(const std::string& line) {
        // Trim whitespace
        std::string trimmed = trim(line);
        
        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#' || trimmed.starts_with("//")) {
            return;
        }

        // Find '=' separator
        size_t pos = trimmed.find('=');
        if (pos == std::string::npos) {
            return; // Invalid line
        }

        std::string key = trim(trimmed.substr(0, pos));
        std::string value = trim(trimmed.substr(pos + 1));

        values_[key] = value;
    }

    std::string trim(const std::string& str) const {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }

    template<typename T>
    T convertValue(const std::string& str) const {
        std::istringstream iss(str);
        T value;
        iss >> value;
        return value;
    }

    std::unordered_map<std::string, std::string> values_;
};

// Template specialization for string
template<>
inline std::string ConfigFileReader::convertValue<std::string>(const std::string& str) const {
    return str;
}

// Template specialization for bool
template<>
inline bool ConfigFileReader::convertValue<bool>(const std::string& str) const {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
}

} // namespace nexus::utility::cli
