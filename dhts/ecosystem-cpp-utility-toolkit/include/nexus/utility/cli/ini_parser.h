#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <optional>
#include <algorithm>
#include <vector>

namespace nexus::utility::cli {

/**
 * @brief Standard INI file format parser with [sections].
 */
class IniParser {
public:
    /**
     * @brief Loads INI file.
     */
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        std::string current_section;
        std::string line;
        
        while (std::getline(file, line)) {
            parseLine(line, current_section);
        }

        return true;
    }

    /**
     * @brief Gets a value from a section.
     */
    template<typename T>
    T get(const std::string& section, const std::string& key, T default_value = T{}) const {
        std::string full_key = section + "." + key;
        auto it = values_.find(full_key);
        if (it == values_.end()) {
            return default_value;
        }

        return convertValue<T>(it->second);
    }

    /**
     * @brief Checks if a section exists.
     */
    bool hasSection(const std::string& section) const {
        return sections_.find(section) != sections_.end();
    }

    /**
     * @brief Checks if a key exists in a section.
     */
    bool has(const std::string& section, const std::string& key) const {
        std::string full_key = section + "." + key;
        return values_.find(full_key) != values_.end();
    }

    /**
     * @brief Gets all sections.
     */
    std::vector<std::string> sections() const {
        std::vector<std::string> result;
        for (const auto& section : sections_) {
            result.push_back(section);
        }
        return result;
    }

    /**
     * @brief Gets all keys in a section.
     */
    std::vector<std::string> keys(const std::string& section) const {
        std::vector<std::string> result;
        std::string prefix = section + ".";
        
        for (const auto& [key, _] : values_) {
            if (key.starts_with(prefix)) {
                result.push_back(key.substr(prefix.length()));
            }
        }
        return result;
    }

private:
    void parseLine(const std::string& line, std::string& current_section) {
        std::string trimmed = trim(line);
        
        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            return;
        }

        // Check for section header
        if (trimmed[0] == '[' && trimmed.back() == ']') {
            current_section = trimmed.substr(1, trimmed.length() - 2);
            sections_.insert(current_section);
            return;
        }

        // Parse key=value
        size_t pos = trimmed.find('=');
        if (pos == std::string::npos) {
            return;
        }

        std::string key = trim(trimmed.substr(0, pos));
        std::string value = trim(trimmed.substr(pos + 1));

        std::string full_key = current_section.empty() ? key : current_section + "." + key;
        values_[full_key] = value;
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
    std::unordered_set<std::string> sections_;
};

// Template specializations
template<>
inline std::string IniParser::convertValue<std::string>(const std::string& str) const {
    return str;
}

template<>
inline bool IniParser::convertValue<bool>(const std::string& str) const {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
}

} // namespace nexus::utility::cli
